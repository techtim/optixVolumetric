#include <optix_world.h>

using namespace optix;

rtBuffer<float4> voxel_buffer;
rtBuffer<float4> color_buffer;

rtDeclareVariable(float4,  sphere, , );

rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, ); 
rtDeclareVariable(float3, shading_normal, attribute shading_normal, ); 
rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );

// object color assigned at intersection time
rtDeclareVariable(float4, obj_color, attribute obj_color, );
rtDeclareVariable(float, cutoff_from, , );
rtDeclareVariable(float, cutoff_to, , );

//
// Box
//

static __device__ void make_box(const float4 & input, float3 & boxmin, float3  & boxmax) {
  float halfWidth = input.w/2;
  boxmin.x = input.x-halfWidth; boxmax.x = input.x+halfWidth;
  boxmin.y = input.y-halfWidth; boxmax.y = input.y+halfWidth;
  boxmin.z = input.z-halfWidth; boxmax.z = input.z+halfWidth;
}

static __device__ float3 boxnormal(const float3 & boxmin, const float3 & boxmax, const float & t)
{
  
  float3 t0 = (boxmin - ray.origin)/ray.direction;
  float3 t1 = (boxmax - ray.origin)/ray.direction;
  float3 neg = make_float3(t==t0.x?1:0, t==t0.y?1:0, t==t0.z?1:0);
  float3 pos = make_float3(t==t1.x?1:0, t==t1.y?1:0, t==t1.z?1:0);
  return pos-neg;
}

RT_PROGRAM void box_intersect(int primIdx)
{

  if (color_buffer[primIdx].w == 0) return;
  else if (color_buffer[primIdx].w < cutoff_from || color_buffer[primIdx].w > cutoff_to) return;

	float3 boxmin, boxmax;
	make_box(voxel_buffer[primIdx], boxmin, boxmax);

	float3 t0 = (boxmin - ray.origin)/ray.direction;
  float3 t1 = (boxmax - ray.origin)/ray.direction;
  float3 near = fminf(t0, t1);
  float3 far = fmaxf(t0, t1);
  float tmin = fmaxf( near );
  float tmax = fminf( far );

  if(tmin <= tmax) {
    bool check_second = true;
    if( rtPotentialIntersection( tmin ) ) {
       shading_normal = geometric_normal = boxnormal(boxmin, boxmax, tmin );
       obj_color = color_buffer[primIdx];
       if(rtReportIntersection(0))
         check_second = false;
    } 
    if(check_second) {
      if( rtPotentialIntersection( tmax ) ) {
        shading_normal = geometric_normal = boxnormal(boxmin, boxmax, tmax );
        obj_color = color_buffer[primIdx];
        rtReportIntersection(0);
      }
    }
  }
}

RT_PROGRAM void box_bounds (int primIdx, float result[6])
{
	float3 boxmin, boxmax;
	make_box(voxel_buffer[primIdx], boxmin, boxmax);
  optix::Aabb* aabb = (optix::Aabb*)result;
  aabb->set(boxmin, boxmax);
}

//
// Sphere 
//

RT_PROGRAM void sphere_intersect(int primIdx)
{

  if (color_buffer[primIdx].w == 0) return;
  else if (color_buffer[primIdx].w < cutoff_from || color_buffer[primIdx].w > cutoff_to) return;

  float3 center = make_float3(voxel_buffer[primIdx]);
  float radius = voxel_buffer[primIdx].w/2;

  float3 V = center - ray.origin;
  float b = dot(V, ray.direction);
  float disc = b*b + radius*radius - dot(V, V);
  if (disc > 0.0f) {
    disc = sqrtf(disc);

#define FASTONESIDEDSPHERES 1
#if defined(FASTONESIDEDSPHERES)
    // only calculate the nearest intersection, for speed
    float t1 = b - disc;
    if (rtPotentialIntersection(t1)) {
      shading_normal = geometric_normal = (t1*ray.direction - V) / radius;
      obj_color = color_buffer[primIdx]; // uniform color for the entire object
      rtReportIntersection(0);
    }
#else
    float t2 = b + disc;
    if (rtPotentialIntersection(t2)) {
      shading_normal = geometric_normal = (t2*ray.direction - V) / radius;
      // float3 offset = shading_normal * scene_epsilon;
      obj_color = color_buffer[primIdx]; // uniform color for the entire object
      rtReportIntersection(0);
    }

    float t1 = b - disc;
    if (rtPotentialIntersection(t1)) {
      shading_normal = geometric_normal = (t1*ray.direction - V) / radius;
      obj_color = color_buffer[primIdx]; // uniform color for the entire object
      rtReportIntersection(0);
    }
#endif
  }
}



RT_PROGRAM void sphere_bounds (int primIdx, float result[6])
{
  const float3 cen = make_float3( voxel_buffer[primIdx] );
  const float3 rad = make_float3( voxel_buffer[primIdx].w/2 );

  optix::Aabb* aabb = (optix::Aabb*)result;
  
  if( rad.x > 0.0f  && !isinf(rad.x) ) {
    aabb->m_min = cen - rad;
    aabb->m_max = cen + rad;
  } else {
    aabb->invalidate();
  }
}
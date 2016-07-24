//
//  volumetric.cu
//  optixVolumetric
//
//  Created by Tim Tavlintsev (TVL)
//
//

#include <optix_world.h>
#include <optixu/optixu_math_namespace.h>
#include "commonStructs.h"
#include "helpers.h"

using namespace optix;

struct PerRayData_radiance
{
  float3 result;
  float importance;
  int depth;
};

struct PerRayData_shadow
{
  float3 attenuation;
};


rtBuffer<BasicLight>                 lights;
rtDeclareVariable(float3,            ambient_light_color, , );
rtDeclareVariable(unsigned int,      radiance_ray_type, , );
rtDeclareVariable(unsigned int,      shadow_ray_type, , );
rtDeclareVariable(rtObject,          top_object, , );
rtDeclareVariable(rtObject,          top_shadower, , );
rtDeclareVariable(float,             scene_epsilon, , );

rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, ); 
rtDeclareVariable(float3, shading_normal,   attribute shading_normal, ); 
rtDeclareVariable(float4, obj_color, attribute obj_color, );

rtDeclareVariable(float3, shadow_attenuation, , );
rtDeclareVariable(int, show_shadows, , );

rtDeclareVariable(optix::Ray,          ray,        rtCurrentRay, );
rtDeclareVariable(float,               t_hit,      rtIntersectionDistance, );
rtDeclareVariable(PerRayData_radiance, prd,        rtPayload, );
rtDeclareVariable(PerRayData_shadow,   prd_shadow, rtPayload, );

RT_PROGRAM void any_hit_shadow()
{

// #define TRANSPARENT 1
#ifndef TRANSPARENT
  // this material is opaque, so it fully attenuates all shadow rays
  prd_shadow.attenuation = optix::make_float3(0);
  rtTerminateRay();

#else
  // Attenuates shadow rays for shadowing transparent objects
  float3 world_normal = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
  float nDi = fabs(dot(world_normal, ray.direction));

  prd_shadow.attenuation *= 1-fresnel_schlick(nDi, 5, 1-shadow_attenuation, make_float3(1));
  if(optix::luminance(prd_shadow.attenuation) < importance_cutoff)
    rtTerminateRay();
  else
    rtIgnoreIntersection();
#endif

}


RT_PROGRAM void closest_hit_radiance()
{
  float3 world_shading_normal = optix::normalize( 
      rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
  float3 world_geometric_normal = optix::normalize( 
      rtTransformNormal( RT_OBJECT_TO_WORLD, geometric_normal ) );
  float3 normal = optix::faceforward(
      world_shading_normal, -ray.direction, world_geometric_normal );

  float3 hit_point = ray.origin + t_hit * ray.direction;
  
  float3 Kd = make_float3( obj_color ); 

  // ambient contribution
  float3 result = Kd * ambient_light_color;

  // compute direct lighting
  unsigned int num_lights = lights.size();
  for(int i = 0; i < num_lights; ++i) {
    BasicLight light = lights[i];
    float Ldist = optix::length(light.pos - hit_point);
    float3 L = optix::normalize(light.pos - hit_point);
    float nDl = optix::dot( normal, L);

    // cast shadow ray
    float3 light_attenuation = make_float3(static_cast<float>( nDl > 0.0f ));
    if ( nDl > 0.0f && light.casts_shadow && show_shadows == 1) {
      PerRayData_shadow shadow_prd;
      shadow_prd.attenuation = make_float3(1.0f);
      optix::Ray shadow_ray = optix::make_Ray(
          hit_point, L, shadow_ray_type, scene_epsilon, Ldist );
      rtTrace(top_shadower, shadow_ray, shadow_prd);
      light_attenuation = shadow_prd.attenuation;
    }

    // If not completely shadowed, light the hit point
    if( fmaxf(light_attenuation) > 0.0f ) {
      float3 Lc = light.color * light_attenuation;
      result += Kd * nDl * Lc;
    }
  }

  // pass the color back up the tree
  prd.result = result;
}

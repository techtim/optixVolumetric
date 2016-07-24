//
//  VolumetricScene.h
//  optixVolumetric
//
//  Created by Tim Tavlintsev (TVL)
//
//

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>
#include "commonStructs.h"
#include "Common.h"

using namespace optix;
using namespace std;

class VolumetricScene : public SampleScene
{
public:
	VolumetricScene( uint3 volumeSize, string filename, bool bSphereVoxel = false);

	// Part From SampleScene
	virtual void initScene( InitialCameraData& camera_data );
	virtual void trace( const RayGenCameraData& camera_data );
	virtual Buffer getOutputBuffer();
	virtual bool keyPressed(unsigned char key, int x, int y);

	void createGeometry();
	void createGrid(const uint3 & volumeSize, bool bSphereVoxel=false);

	void loadDataset(const string & filename);

	void setGradient(const float3 & grad_begin, const float3 & grad_end) {
		m_gradient_begin = grad_begin; m_gradient_end = grad_end; 
	}
	void setCutoff(const float & cut_from, const float & cut_to);

private:
	Buffer       	m_top_level_buffer; 
	GeometryGroup	m_geometry_group;

	unsigned int  	m_num_instances;
	Acceleration  	m_sphere_accel;
	Buffer        	m_voxel_buffer;
	Buffer        	m_color_buffer;
	
	Geometry      	m_sphereG;
	GeometryGroup 	m_sphereGG;
	bool 			m_bSphereVoxel;
	string 			data_filename;

	bool 			m_bShowShadows;

	uint3 			m_volume_size;
	float3 			m_gradient_begin, m_gradient_end;
	float 			m_cutoff_from, m_cutoff_to;

	static unsigned int WIDTH;
	static unsigned int HEIGHT;
};

unsigned int VolumetricScene::WIDTH  = 800u;
unsigned int VolumetricScene::HEIGHT = 800u;

VolumetricScene::VolumetricScene( uint3 volumeSize, string filename, bool bSphereVoxel)
	: data_filename(filename), m_volume_size(volumeSize), m_bSphereVoxel(bSphereVoxel), m_bShowShadows(true)
{
	// default values
	setGradient(make_float3(0.0f, 0.1f, 8.f), make_float3(0.2f, 0.8f, 0.f));
	setCutoff(0, 255);
}

void VolumetricScene::initScene( InitialCameraData& camera_data )
{
	try {
		// Setup state
		m_context->setRayTypeCount( 2 );
		m_context->setEntryPointCount( 1 );
		m_context->setStackSize(1200);

		m_context["radiance_ray_type"]->setUint( 0u );
		m_context["shadow_ray_type"]->setUint( 1u );
		m_context["scene_epsilon"]->setFloat( 1.e-4f ); // 0.0001
		m_context["shadow_attenuation"]->setFloat( 0.3f, 0.3f, 0.3f );
		m_context["show_shadows"]->setInt(m_bShowShadows? 1 : 0);

		Variable output_buffer = m_context["output_buffer"];

		output_buffer->set(
				createOutputBuffer(RT_FORMAT_UNSIGNED_BYTE4, WIDTH, HEIGHT ) );

		// Set up camera
		camera_data = InitialCameraData( make_float3( 0.0f, 0.0f, static_cast<int>(m_volume_size.x)*-2 ), // eye
																		 make_float3( 0.0f, 0.0f,  0.0f ), // lookat
																		 make_float3( 0.0f, 1.0f,  0.0f ), // up
																		 60.0f );                          // vfov

		// Declare camera variables.  The values do not matter, they will be
		// overwritten in trace.
		m_context["eye"]->setFloat( make_float3( 0.0f, 0.0f, 0.0f ) );
		m_context["U"]->setFloat( make_float3( 0.0f, 0.0f, 0.0f ) );
		m_context["V"]->setFloat( make_float3( 0.0f, 0.0f, 0.0f ) );
		m_context["W"]->setFloat( make_float3( 0.0f, 0.0f, 0.0f ) );

		// Ray gen program
		string ptx_path( ptxpath( "volumetric", "pinhole_camera.cu" ) );
		Program ray_gen_program = m_context->createProgramFromPTXFile( 
				ptx_path, "pinhole_camera" );
		m_context->setRayGenerationProgram( 0, ray_gen_program );

		// Exception
		Program exception_program = m_context->createProgramFromPTXFile( 
				ptx_path, "exception" );
		m_context->setExceptionProgram( 0, exception_program );
		m_context["bad_color"]->setFloat( 1.0f, 1.0f, 0.0f );

		// Miss program
		m_context->setMissProgram( 0, m_context->createProgramFromPTXFile( 
					ptxpath( "volumetric", "constantbg.cu" ), "miss" ) );
		m_context["bg_color"]->setFloat( 
				make_float3(100.0f/255.0f, 100.0f/255.0f, 100.0f/255.0f));

		// Setup lights
		m_context["ambient_light_color"]->setFloat(0.3f,0.3f,0.3f);
		BasicLight lights[] = { 
			//{ { 0.0f, 8.0f, -5.0f }, { .4f, .4f, .4f }, 1 },
			{ make_float3( -600.0f, 100.0f, -1200.0f ), make_float3( 0.8f, 0.8f, 0.8f ), 1 },
			{ make_float3( 600.0f,   0.0f,  1200.0f ), make_float3( 0.8f, 0.8f, 0.8f ), 1 }
		};

		Buffer light_buffer = m_context->createBuffer(RT_BUFFER_INPUT);
		light_buffer->setFormat(RT_FORMAT_USER);
		light_buffer->setElementSize(sizeof(BasicLight));
		light_buffer->setSize( sizeof(lights)/sizeof(lights[0]) );
		memcpy(light_buffer->map(), lights, sizeof(lights));
		light_buffer->unmap();
		m_context["lights"]->set(light_buffer);

		// --------------------------
		// Create scene geom
		createGeometry();

		// And load data for it
		loadDataset(data_filename);

		// Finalize
		m_context->validate();
		m_context->compile();

	} catch( Exception& e ){
		sutilReportError( e.getErrorString().c_str() );
		exit(1);
	}
}

// Fill grid with objects
void VolumetricScene::createGeometry()
{
	// Count all voxels
	m_num_instances = m_volume_size.x * m_volume_size.y * m_volume_size.z;

	string ptx_path( ptxpath( "volumetric", "voxel.cu" ) );
	m_sphereG = m_context->createGeometry();
	m_sphereG->setBoundingBoxProgram( m_context->createProgramFromPTXFile( ptx_path, m_bSphereVoxel? "sphere_bounds" : "box_bounds" ) );
	m_sphereG->setIntersectionProgram( m_context->createProgramFromPTXFile( ptx_path,m_bSphereVoxel? "sphere_intersect" : "box_intersect" ) );

	// Allocate voxels buffer and bind
	m_voxel_buffer = m_context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, m_num_instances );
	float4 *spheres = reinterpret_cast<float4 *>( m_voxel_buffer->map() );
 
	int halfW = m_volume_size.x/2, halfH = m_volume_size.y/2, halfD = m_volume_size.z/2;

	// Fill spheres buffer
	unsigned int i = 0u;
	for (int x=0;x<m_volume_size.x;x++) 
		for (int y=0;y<m_volume_size.y;y++)
			for (int z=0;z<m_volume_size.z;z++) {
				spheres[i++] = make_float4(static_cast<float>(x-halfW), static_cast<float>(y-halfH), static_cast<float>(z-halfD), 1.f );
			}
	
	m_voxel_buffer->unmap();

	m_sphereG->setPrimitiveCount( m_num_instances );

	m_context["voxel_buffer"]->setBuffer( m_voxel_buffer );

	// Sphere material  
	Material sphere_mat = m_context->createMaterial();
	sphere_mat->setClosestHitProgram( 0, m_context->createProgramFromPTXFile(ptxpath("volumetric", "volumetric.cu"),"closest_hit_radiance") );
	sphere_mat->setAnyHitProgram( 1, m_context->createProgramFromPTXFile(ptxpath("volumetric", "volumetric.cu"),"any_hit_shadow") );

	// Place geometry and material into hierarchy
	vector<GeometryInstance> GIs;
	GIs.push_back(m_context->createGeometryInstance(m_sphereG, &sphere_mat, &sphere_mat+1) );
	
	m_sphereGG = m_context->createGeometryGroup( GIs.begin(), GIs.end() );
	m_sphereGG->setAcceleration( m_sphere_accel = m_context->createAcceleration( "MedianBvh", "Bvh" ) );

	m_context["top_object"]->set( m_sphereGG );
	m_context["top_shadower"]->set( m_sphereGG );
}

void VolumetricScene::loadDataset(const string & filename) {
	if (filename == "") 
		throw init_exception("[Error] Wrong path to Dataset");

	string data; 
	// Open the file for reading, binary mode 
	ifstream ifFile (filename, ios_base::binary);

	// Read in all the data from the file into one string object
	data.assign(istreambuf_iterator <char> (ifFile),
				istreambuf_iterator <char> ());

	if (data.size() < m_volume_size.x*m_volume_size.y*m_volume_size.z)
		throw init_exception("[Error] Not enough values in Dataset for defined volume");

	// Create color buffer for spheres and bind to pointer
	m_color_buffer = m_context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, data.size());
	float4 *colors = reinterpret_cast<float4 *>( m_color_buffer->map() );
	
	// fill buffer with values from file
	for(unsigned int i = 0; i<data.size(); i++) {
		colors[i] = make_float4(interpolate(m_gradient_begin, m_gradient_end, data[i]/255.f), data[i]);
	}
	// unbind buffer
	m_color_buffer->unmap();

	m_context["color_buffer"]->setBuffer(m_color_buffer);
}

bool VolumetricScene::keyPressed(unsigned char key, int x, int y)
{
	bool bUpdate = false;
	switch(key) {
		case '-':
			if (m_cutoff_from>0) m_cutoff_from-=1.f;
			bUpdate = true;
			break;
		case '=':
			if (m_cutoff_from<255) m_cutoff_from+=1.f;
			bUpdate = true;
			break;
		case '[':
			if (m_cutoff_to>0) m_cutoff_to-=1.f;
			bUpdate = true;
			break;
		case ']':
			if (m_cutoff_to<255) m_cutoff_to+=1.f;
			bUpdate = true;
			break;
		case 's':
			m_bShowShadows = !m_bShowShadows;
			m_context["show_shadows"]->setInt(m_bShowShadows? 1 : 0);
			return true;
			break;
	}

	if (bUpdate) {
		setCutoff(m_cutoff_from, m_cutoff_to);
		return true;
	}

	return false;
}

void VolumetricScene::setCutoff(const float & cut_from, const float & cut_to) { 
	m_cutoff_from = cut_from; m_cutoff_to = cut_to;
	m_context["cutoff_from"]->setFloat(m_cutoff_from);
	m_context["cutoff_to"]->setFloat(m_cutoff_to);
	cout << "Window from " << m_cutoff_from << " to " << m_cutoff_to << endl;
}

Buffer VolumetricScene::getOutputBuffer()
{
	return m_context["output_buffer"]->getBuffer();
}

void VolumetricScene::trace( const RayGenCameraData& camera_data )
{
	m_context["eye"]->setFloat( camera_data.eye );
	m_context["U"]->setFloat( camera_data.U );
	m_context["V"]->setFloat( camera_data.V );
	m_context["W"]->setFloat( camera_data.W );


	Buffer buffer = m_context["output_buffer"]->getBuffer();
	RTsize buffer_width, buffer_height;
	buffer->getSize( buffer_width, buffer_height );

	m_context->launch( 0, 
			static_cast<unsigned int>(buffer_width),
			static_cast<unsigned int>(buffer_height)
			);
}

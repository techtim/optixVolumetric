//
//  volumetric.cpp
//  optixVolumetric
//
//  Created by Tim Tavlintsev (TVL)
//
//

#include <GLUTDisplay.h>
#include "VolumetricScene.h"

using namespace optix;
using namespace std;


void printUsageAndExit( const string& argv0, bool doExit = true )
{
	cerr
		<< "Usage  : " << argv0 << " [options]\n"
		<< "App options:\n"
		<< "  --sphere                               	Show voxels as spheres \n"
		<< "  --size=<X>x<Y>x<Z>						Set volume dimensions \n" 
		<< "  --dataset=<path_to_bin_data>              Set path for volumetric data \n"
		<< "  --gradient-start=0xff0000ff 				Set begin gradient \n" 
		<< "  --gradient-end=0x00ff00ff 				Set end gradient \n"
		<< "  --window=\"[<val>, <val>]\"				Set filtering window for values from dataset"
		// << " buffers (defaults to 4)\n"
		<< endl;
		// GLUTDisplay::printUsage();
	cerr
		<< "App keystrokes:\n"
		<< "  '-' / '=' Decrease/increase window values from \n"
		<< "  '[' / ']' Decrease/increase window values to \n"
		<< "     's'    Show shadows"
		<< endl;

	if ( doExit ) exit(1);
}

int main( int argc, char** argv )
{
	GLUTDisplay::init( argc, argv );

	// Temporal values for config
	uint3 volumeSize = make_uint3(200, 200, 200);
	string filename;
	vector<float> cut;
	float3 gradient_start = make_float3(0.9f,0,0), gradient_end = make_float3(0,0,0.9f);
	bool bSphereVoxel = false;

	// Process commandline
	for(int i = 1; i < argc; ++i) {
		string arg(argv[i]);
		std::string arg_prefix, arg_value;
		{
			size_t pos = arg.find_first_of( '=' );
			if (pos != std::string::npos) {
				arg_prefix = arg.substr( 0, pos + 1 );
				arg_value = arg.substr( pos + 1 );
			}
		}
		if(arg_prefix =="--window=") {
			regex number_re("[0-9]+");;
			sregex_iterator re(arg_value.begin(), arg_value.end(), number_re);
  			sregex_iterator end;
  			while (re != end) {
    			smatch base_match = *re;
        		cut.push_back(strTo<float>(base_match.str()));
        		re++;
        	}
        	if (cut.size() > 1) {
        		cout << "Window from " << cut[0] << " to " << cut[1] << endl;
        	}

		// Check for dataset file
		} else if ( arg_prefix == "--dataset=" ) {
			filename = arg_value;
			ifstream the_file( filename );
			// Always check to see if file opening succeeded
			if ( !the_file.is_open() ) {
				cout<<"Could not open file " << filename << "\n";
				filename = "";
			} else {
				the_file.close();
				cout<<"Data file: " << filename << "\n";
			}

		// Check for volume dimensions
		} else if (arg_prefix == "--size=") {
			vector<string> volumesInput = splitString(arg_value, "x");
			if (volumesInput.size() == 3) {
				volumeSize.x = strTo<unsigned int>(volumesInput[0]);
				volumeSize.y = strTo<unsigned int>(volumesInput[1]);
				volumeSize.z = strTo<unsigned int>(volumesInput[2]);
			}
			cout << "Volume Size: " << volumeSize.x << " x " << volumeSize.y << " x " << volumeSize.z << endl;
		
		// Read gradient values
		} else if (arg_prefix == "--gradient-start=") {
			gradient_start = strToColor(arg_value);
			cout << "gradient-start: " << gradient_start.x << " x " << gradient_start.y << " x " << gradient_start.z << endl;
		} else if (arg_prefix == "--gradient-end=") {
			gradient_end = strToColor(arg_value);
			cout << "gradient-start: " << gradient_end.x << " x " << gradient_end.y << " x " << gradient_end.z << endl;

		// Check for sphere redrering flag
		} else if (arg == "--sphere") {
			bSphereVoxel = true;
		}
		else {
			cerr << "Unknown option '" << arg << "'\n";
			printUsageAndExit( argv[0] );
		}
	}

	if( !GLUTDisplay::isBenchmark() ) printUsageAndExit( argv[0], false );

	try {
		VolumetricScene scene( volumeSize, filename, bSphereVoxel);
		if (cut.size() > 1) 
			scene.setCutoff(cut[0], cut[1]);
		scene.setGradient(gradient_start, gradient_end);
	
		GLUTDisplay::run( "VolumetricScene", &scene );
	} catch (init_exception & exception) {
		cout << exception.what() << endl;
		exit(1);
	} catch( Exception& e ){
		sutilReportError( e.getErrorString().c_str() );
		exit(1);
	}
	
	return 0;
}

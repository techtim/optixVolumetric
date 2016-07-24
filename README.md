# optixVolumetric
Nvidia OptiX Volumetric rendering experiment

Building
========
Clone this repo to your OptiX/SDK folder
add line OptiX/SDK/CMakeLists.txt -> " add_subdirectory(optixVolumetric) "
  cd OptiX/SDK 
  cmake <path-to-build-folder>
  cd <path-to-build-folder>/optixVolumetric 
  make
  
Running
========
./optixVolumetric --dataset=/OptiX/SDK/optixVolumetric/dataset.raw --size=96x99x96  --window="[30, 200]" --gradient-start=0xff00aaff --gradient-end=0x00ff00ff

Available flags:
  --sphere                      Show voxels as spheres
  --size=<X>x<Y>x<Z>						Set volume dimensions 
  --dataset=<path_to_bin_data>  Set path for volumetric data
  --gradient-start=0xff0000ff 	Set begin gradient
  --gradient-end=0x00ff00ff 		Set end gradient
  --window="[<val>, <val>]"		  Set filtering window for values from dataset

# multiChannelOSPModule
usage:  
- put entire folder under ospray/module 
- in cmake set OSPRAY_MODULE_MULTIVARIANT_RENDERER to ON 


multichannel renderer:   
a multi-channel renderer modified from scivis renderer, example viewer under example/

build with:
 - ospray 2.7.0
 - embree 3.13.1
 - openvkl 1.0.1
 - rkcommon 1.7.0
 - ispc 1.16.0
 
 run with:
 ./ospTutorial_mtvCpp <raw vol filename> x y z n_of_channels <image Folder Path>

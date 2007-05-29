/************************************************************************/
/*     File: file_lib.cc                                                	*/ 
/*     Date: August 1996                                                */
/*       By: Eric Zbinden					  	*/
/*      For: NASA Ames Research Center, Intelligent Mechanisms Group  	*/
/* Function: Read, write & initialize, Files, buffers & struct		*/
/************************************************************************/

// ability to also read old-style config file
#define READ_OLD_CONFIG_FILE
// force to write old-style config file INSTEAD of new-style
#define WRITE_OLD_CONFIG_FILE

#define NAME_LENGTH 1024

#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <boost/version.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "file_lib.h"
#include "MOC/Metadata.h"

#include <vw/Core/Exception.h>
#include <vw/Image/ImageView.h>
#include <vw/FileIO.h>

using namespace std; /* C standard library */
using namespace vw;
using namespace vw::camera;

void write_orbital_reference_model(std::string filename, 
                                   vw::camera::CameraModel const& cam1, 
                                   vw::camera::CameraModel const& cam2) {
  FILE *outflow = stdout;
  const char* resource_path = "/irg/projects/MOC/resources/OrbitViz";

  printf("Writing Orbital Visualization VRML file to disk.\n");
  
  /* open output file */
  if((outflow = fopen (filename.c_str(), "w" )) == 0) { 
    throw IOErr() << "An error occured while opening the Orbital Reference VRML file for writing.";
  }

  fprintf(outflow, "#VRML V1.0 ascii\n#\n");              

  fprintf(outflow, "# File generated by the NASA Ames Stereo Pipeline\n#\n");
  fprintf(outflow, "# Michael Broxton and Larry Edwards\n");
  fprintf(outflow, "# Intelligent Robotics Group, NASA Ames Research Center\n\n");
  fprintf (outflow, "Separator {\n");

  fprintf (outflow, "  #Mars coordinate system\n");
  fprintf (outflow, "  Separator {\n");
  fprintf (outflow, "    Separator {\n");
  fprintf (outflow, "      Scale { scaleFactor 4000 4000 4000 }\n");
  fprintf (outflow, "      File { name \"%s/refFrame.wrl\" }\n", 
	            resource_path);
  fprintf (outflow, "    } Separator {\n");
  fprintf (outflow, "      File { name \"%s/Mars.wrl\" }\n", resource_path);
  fprintf (outflow, "    }\n  }\n\n");

  /* 
   * First camera frame
   */
  fprintf(outflow, "  # ----------------------------------------------------------\n");
  fprintf(outflow, "  # Camera 1 coordinate system at t0\n");
  fprintf(outflow, "  # ----------------------------------------------------------\n");
  Vector3 pos1 = cam1.camera_center(Vector2(0,0));    // Camera position 
  Quaternion<double> quat1 = cam1.camera_pose(Vector2(0,0)); // Camera Pose
  Matrix<double> rot1(4,4);
  rot1.set_identity();
  quat1.rotation_matrix(rot1);

  fprintf (outflow, "  Separator {\n");
  fprintf (outflow, "    Translation { translation %f %f %f }\n", 
	   pos1(0)/1000.0, pos1(1)/1000.0, pos1(2)/1000.0);
  fprintf (outflow, "    MatrixTransform { matrix\n");
  fprintf (outflow, "      %f %f %f %f\n", rot1(0,0), rot1(0,1), rot1(0,2), rot1(0,3));
  fprintf (outflow, "      %f %f %f %f\n", rot1(1,0), rot1(1,1), rot1(1,2), rot1(1,3));
  fprintf (outflow, "      %f %f %f %f\n", rot1(2,0), rot1(2,1), rot1(2,2), rot1(2,3));
  fprintf (outflow, "      %f %f %f %f\n", rot1(3,0), rot1(3,1), rot1(3,2), rot1(3,3));
  fprintf (outflow, "    }\n");
  fprintf (outflow, "    Separator {\n");
  fprintf (outflow, "      Scale { scaleFactor 1000 1000 1000 }\n");
  fprintf (outflow, "      File  { name \"%s/refFrame.wrl\" }\n", 
                    resource_path);
  fprintf (outflow, "    }\n  }\n");


  /* 
   * Second camera frame
   */
  fprintf(outflow, "  # ----------------------------------------------------------\n");
  fprintf(outflow, "  # Camera 2 coordinate system at t0\n");
  fprintf(outflow, "  # ----------------------------------------------------------\n");
  Vector3 pos2 = cam2.camera_center(Vector2(0,0));    /* Camera position */
  vw::Quaternion<double> quat2 = cam2.camera_pose(Vector2(0,0)); /* Camera pose */
  vw::Matrix<double> rot2(4,4);
  rot2.set_identity();
  quat2.rotation_matrix(rot2);

  fprintf (outflow, "  Separator {\n");
  fprintf (outflow, "    Translation { translation %f %f %f } \n", 
           pos2(0)/1000.0, pos2(1)/1000.0, pos2(2)/1000.0);
  fprintf (outflow, "    MatrixTransform { matrix\n");
  fprintf (outflow, "      %f %f %f %f\n", rot2(0,0), rot2(0,1), rot2(0,2), rot2(0,3));
  fprintf (outflow, "      %f %f %f %f\n", rot2(1,0), rot2(1,1), rot2(1,2), rot2(1,3));
  fprintf (outflow, "      %f %f %f %f\n", rot2(2,0), rot2(2,1), rot2(2,2), rot2(2,3));
  fprintf (outflow, "      %f %f %f %f\n", rot2(3,0), rot2(3,1), rot2(3,2), rot2(3,3));
  fprintf (outflow, "    }\n");
  fprintf (outflow, "    Separator {\n");
  fprintf (outflow, "      Scale { scaleFactor 1000 1000 1000 }\n");
  fprintf (outflow, "      File  { name \"%s/refFrame.wrl\" }\n", 
                    resource_path);
  fprintf (outflow, "    }\n  }\n");
  fprintf (outflow, "}\n\n");
  
  printf("\t%s written successfully\n\n", filename.c_str());
  fclose(outflow);
}

/************************************************************************/
/*									*/
/*	             	     STRUCTURES					*/
/*									*/
/************************************************************************/

//NOTE: this struct and the following functions support option scaling because boost's program_options does not
struct AugmentingDescription {
  const char* name;
  void* data;
  bool needs_scale;
  double scale;
};

enum OptionType {
  OPTION_TYPE_INT,
  OPTION_TYPE_FLOAT,
  OPTION_TYPE_DOUBLE
};

// Determine type of option.
OptionType option_type(po::option_description& d) {
  OptionType type = OPTION_TYPE_INT;
  
  boost::shared_ptr<const po::typed_value<int> > intp = boost::dynamic_pointer_cast<const po::typed_value<int> >(d.semantic());
  boost::shared_ptr<const po::typed_value<float> > floatp = boost::dynamic_pointer_cast<const po::typed_value<float> >(d.semantic());
  boost::shared_ptr<const po::typed_value<double> > doublep = boost::dynamic_pointer_cast<const po::typed_value<double> >(d.semantic());
  
  if(intp.get())
    type = OPTION_TYPE_INT;
  else if(floatp.get())
    type = OPTION_TYPE_FLOAT;
  else if(doublep.get())
    type = OPTION_TYPE_DOUBLE;
  else {
    std::cerr << "Error: Option " << d.long_name() << " is not a supported type." << std::endl;
    exit(EXIT_FAILURE);
  }
  if((intp.get() ? 1 : 0) + (floatp.get() ? 1 : 0) + (doublep.get() ? 1 : 0) > 1) {
    std::cerr << "Error: Option " << d.long_name() << " is of more than one supported type." << std::endl;
    exit(EXIT_FAILURE);
  }
  
  return type;
}

// Scale members of DFT struct appropriately.
void scale_dft_struct(po::options_description *desc, std::vector<AugmentingDescription> *adesc, po::variables_map *vm) {
  OptionType type;
  std::vector<AugmentingDescription>::iterator i;
  for(i = adesc->begin(); i != adesc->end(); i++) {
    // If the option should not be scaled...
    if(!(*i).needs_scale)
      continue;
    // If the value is the default, then we should not scale it.
    if((*vm)[(*i).name].defaulted())
      continue;
#if BOOST_VERSION == 103200
    po::option_description d = desc->find((*i).name);
#else
    po::option_description d = desc->find((*i).first, false);
#endif
    type = option_type(d);
    switch(type) {
    case OPTION_TYPE_INT:
      *((int*)((*i).data)) = (int)((double)(*((int*)((*i).data))) * (*i).scale);
      break;
    case OPTION_TYPE_FLOAT:
      *((float*)((*i).data)) = (float)((double)(*((float*)((*i).data))) * (*i).scale);
      break;
    case OPTION_TYPE_DOUBLE:
      *((double*)((*i).data)) = *((double*)((*i).data)) * (*i).scale;
      break;
    default:
      std::cerr << "Unexpected type!" << std::endl;
      break;
    }
  }
}

// Undo scaling of members of DFT struct.
void unscale_dft_struct(po::options_description *desc, std::vector<AugmentingDescription> *adesc) {
  OptionType type;
  std::vector<AugmentingDescription>::iterator i;
  for(i = adesc->begin(); i != adesc->end(); i++) {
    // If the option should not be scaled...
    if(!(*i).needs_scale)
      continue;
#if BOOST_VERSION == 103200
    po::option_description d = desc->find((*i).name);
#else
    po::option_description d = desc->find((*i).first, false);
#endif
    type = option_type(d);
    switch(type) {
    case OPTION_TYPE_INT:
      *((int*)((*i).data)) = (int)((double)(*((int*)((*i).data))) / (*i).scale);
      break;
    case OPTION_TYPE_FLOAT:
      *((float*)((*i).data)) = (float)((double)(*((float*)((*i).data))) / (*i).scale);
      break;
    case OPTION_TYPE_DOUBLE:
      *((double*)((*i).data)) = *((int*)((*i).data)) / (*i).scale;
      break;
    default:
      std::cerr << "Unexpected type!" << std::endl;
      break;
    }
  }
}

// Build po::options_description desc and associate it with dft and todo.
//NOTE: adesc is to get around
//  1) the fact that we cannot get the store-to pointers back out of the po::value's inside of the po::options_description
//  2) the fact that the boost config-file reader will not allow us to specify a scale-by factor; boost tells us whether a final value is the default value or not, so we DO NOT scale the default by 1/scale (i.e. the specified default value is the one that is visible to the program--it is never scaled)
void associate_dft_struct(DFT_F *dft, TO_DO *todo, po::options_description *desc, std::vector<AugmentingDescription> *adesc) {

  AugmentingDescription ad;

#define ASSOC_INT(X,Y,V,D)             desc->add_options()(X, po::value<int>(&(dft->Y))->default_value(V), D); \
                                       ad.name = X; ad.data = (void*)&(dft->Y); \
                                       ad.needs_scale = false; \
                                       ad.scale = 1.0; adesc->push_back(ad);
#define ASSOC_INT_SCALED(X,Y,V,D,Z)    desc->add_options()(X, po::value<int>(&(dft->Y))->default_value(V), D); \
                                       ad.name = X; ad.data = (void*)&(dft->Y); \
                                       ad.needs_scale = true; \
                                       ad.scale = Z; adesc->push_back(ad);
#define ASSOC_FLOAT(X,Y,V,D)           desc->add_options()(X, po::value<float>(&(dft->Y))->default_value(V), D); \
                                       ad.name = X; ad.data = (void*)&(dft->Y); \
                                       ad.needs_scale = false; \
                                       ad.scale = 1.0; adesc->push_back(ad);
#define ASSOC_FLOAT_SCALED(X,Y,V,D,Z)  desc->add_options()(X, po::value<float>(&(dft->Y))->default_value(V), D); \
                                       ad.name = X; ad.data = (void*)&(dft->Y); \
                                       ad.needs_scale = true; \
                                       ad.scale = Z; adesc->push_back(ad);
#define ASSOC_DOUBLE(X,Y,V,D)          desc->add_options()(X, po::value<double>(&(dft->Y))->default_value(V), D); \
                                       ad.name = X; ad.data = (void*)&(dft->Y); \
                                       ad.needs_scale = false; \
                                       ad.scale = 1.0; adesc->push_back(ad);
#define ASSOC_DOUBLE_SCALED(X,Y,V,D,Z) desc->add_options()(X, po::value<double>(&(dft->Y))->default_value(V), D); \
                                       ad.name = X; ad.data = (void*)&(dft->Y); \
                                       ad.needs_scale = true; \
                                       ad.scale = Z; adesc->push_back(ad);
#define ASSOC_TO_DO(X,Y,V,D)           desc->add_options()(X, po::value<int>(&(todo->Y))->default_value(V), D); \
                                       ad.name = X; ad.data = (void*)&(todo->Y); \
                                       ad.needs_scale = false; \
                                       ad.scale = 1.0; adesc->push_back(ad);

  ASSOC_TO_DO("DO_ALIGNMENT", do_alignment, 1, "Do we do alignment at all?")
  ASSOC_TO_DO("DO_KEYPOINT_ALIGNMENT", keypoint_alignment, 1, "Align images using the keypoint alignment method")
  ASSOC_TO_DO("DO_EPHEMERIS_ALIGNMENT", ephemeris_alignment, 0, "Align images using the ephemeris alignment method")
  ASSOC_TO_DO("DO_EPIPOLAR_ALIGNMENT", epipolar_alignment, 0, "Align images using epipolar constraints")

  ASSOC_TO_DO("DO_FORMAT_IMG_SIZE", format_size, 0, "format the size of the image")
  ASSOC_TO_DO("DO_SLOG", slog, 0, "perform an slog (relpace the emboss)")
  ASSOC_TO_DO("DO_LOG", log, 0, "perform a log (laplacian of gaussian)")
  ASSOC_TO_DO("DO_FIRST_HIST_EQ", eq_hist1, 0, "do the first histogam equalisation")
  ASSOC_TO_DO("DO_EMBOSS", emboss, 0, "do the emboss convolution")
  ASSOC_TO_DO("DO_SECOND_HIST_EQ", eq_hist2, 0, "do the second histogam equalisation")
  ASSOC_TO_DO("AUTO_SET_H_CORR_PARAM", autoSetCorrParam, 0, "uses pyramidal scheme to autom get search param")
  ASSOC_TO_DO("DO_VERT_CAL", vert_cal, 0, "do the vertical calibration")
  ASSOC_TO_DO("WRITE_TEXTURE", w_texture, 0, "write the pgm texture File")
  ASSOC_TO_DO("WRITE_PREPROCESSED", w_preprocessed, 0, "write the preprocessed image file")
  ASSOC_TO_DO("CORR_1ST_PASS", corr_1st_pass, 1, "do the correlation")
  ASSOC_TO_DO("2D_CORRELATION", biDimCorr, 1, "do a 2D correlation by default")
  ASSOC_TO_DO("CORR_CLEAN_UP", corr_clean_up, 0, "do n filtering pass to rm wrong matches")
  ASSOC_TO_DO("WRITE_DEBUG_DISP", w_debug_disp, 0, "write intermediate disp.pgm files")
  ASSOC_TO_DO("WRITE_DISP_STP", w_disp_stp, 1, "write an stp file of the raw disp map")
  ASSOC_TO_DO("WRITE_DISP_PGM", w_disp_pgm, 0, "write an pgm file of the raw disp map")
  ASSOC_TO_DO("WRITE_RAW_DISPARITIES", w_raw_disparity_map, 0, "write raw unscaled disparity values")
  ASSOC_TO_DO("WRITE_PGM_DISPARITIES", w_pgm_disparity_map, 0, "write a pgm file of disparity map unscaled")
  ASSOC_TO_DO("FILL_V_HOLES", fill_v_holes, 0, "fill holes in dispmap with vert algorithm")
  ASSOC_TO_DO("FILL_H_HOLES", fill_h_holes, 0, "fill holes in dispmap with horz algorithm")
  ASSOC_TO_DO("FILL_HOLES_NURBS", fill_holes_NURBS, 0, "fill holes using Larry's NURBS code")
  ASSOC_TO_DO("EXTEND_DISP_LR", extend_lr, 0, "extrapolate disp values (Left/Right)")
  ASSOC_TO_DO("EXTEND_DISP_TB", extend_tb, 0, "extrapolate disp values (Top/Bottom)")
  ASSOC_TO_DO("SMOOTH_DISP", smooth_disp, 0, "smooth the disp map")
  ASSOC_TO_DO("WRITE_FILTERED_DISP_PGM", w_filtered_disp_pgm, 0, "write the filtered disp map in pgm")
  ASSOC_TO_DO("SMOOTH_RANGE", smooth_range, 0, "do a smooth range on the range file")
  ASSOC_TO_DO("DO_DOTCLOUD", dotcloud, 0, "build the dotcloud model")
  ASSOC_TO_DO("DO_LOCAL_LEVEL_TRANSFORM", local_level_transform, 0, "coordinate transfrom: lander to local level to z-up, x-north frame")
  ASSOC_TO_DO("WRITE_DOTCLOUD", w_dotcloud, 0, "write dotcloud file")
  ASSOC_TO_DO("WRITE_MVACS_RANGE", w_vicar_range_maps, 0, "write range maps in mvacs vicar format")
  ASSOC_TO_DO("WRITE_VICAR_XYZ", w_vicar_xyz_map, 0, "write xyz range map in vicar format")
  ASSOC_TO_DO("WRITE_DISP_VICAR", w_disp_vicar, 0, "write disp map in vicar format")
  ASSOC_TO_DO("DO_ALTITUDE_TEXTURE", alt_texture, 0, "create and write a texture f(altitude)")
  ASSOC_TO_DO("DO_3D_MESH", mesh, 0, "do the mesh")
  ASSOC_TO_DO("ADAPTIVE_MESHING", adaptative_meshing, 0, "do not do the adaptative meshing by dft")
  ASSOC_TO_DO("NFF_PLAIN", nff_plain, 0, "save it as a plain model")
  ASSOC_TO_DO("NFF_TXT", nff_txt, 0, "save it as a textured model")
  ASSOC_TO_DO("DOUBLE_SIDED", double_sided, 0, "draw two sided polygons")
  ASSOC_TO_DO("INVENTOR", inventor, 0, "save it as an Inventor file")
  ASSOC_TO_DO("VRML", vrml, 0, "save it as an VRML file")
  ASSOC_TO_DO("WRITE_IVE", write_ive, 1, "save it as an OpenSceneGraph file")
  ASSOC_TO_DO("WRITE_DEM", write_dem, 0, "save it as a DEM file")
  ASSOC_TO_DO("APPLY_MASK", apply_mask, 1, "apply the mask by default")
  ASSOC_TO_DO("WRITE_MASK", w_mask, 0, "do not write the mask file by default")
  ASSOC_TO_DO("WRITE_EXTRAPOLATION_MASK", w_extrapolation_mask, 0, "do not write the extrapolation mask")

  ASSOC_DOUBLE("EPHEM_ALIGN_KERNEL_X", ephem_align_kernel_x, 150, "x coordinate of the ephem. alignmnt kernel")
  ASSOC_DOUBLE("EPHEM_ALIGN_KERNEL_Y", ephem_align_kernel_y, 150, "y coordinate of the ephem. alignmnt kernel")
  ASSOC_INT("EPHEM_ALIGN_KERNEL_WIDTH", ephem_align_kernel_width, 40, "Width of the ephemeris alignment kernel")
  ASSOC_INT("EPHEM_ALIGN_KERNEL_HEIGHT",ephem_align_kernel_height, 40, "Height of the ephemeris alignment kernel")

  ASSOC_INT("H_KERNEL", h_kern, 0, "kernel width first pass")
  ASSOC_INT("V_KERNEL", v_kern, 0, "kernel height first pass")  
  ASSOC_INT("CORR_MARGIN",corr_margin, 0, "extra margin for search window")
  ASSOC_INT("H_CORR_MAX", h_corr_max, 0, "correlation window size max x")
  ASSOC_INT("H_CORR_MIN", h_corr_min, 0, "correlation window size min x")
  ASSOC_INT("CROP_X_MIN", crop_x_min, 0, "cropping coordonate")
  ASSOC_INT("CROP_X_MAX", crop_x_max, 0, "")
  ASSOC_INT("CROP_Y_MIN", crop_y_min, 0, "")
  ASSOC_INT("CROP_Y_MAX", crop_y_max, 0, "")
  ASSOC_INT("V_CORR_MIN", v_corr_min, 0, "automatic img alignment parameters")
  ASSOC_INT("V_CORR_MAX", v_corr_max, 0, "min max vertical picture shift interval")
  ASSOC_INT("AUTO_SET_V_CORR_PARAM", autoSetVCorrParam, 0, "goes with autoSetCorrParam")
  /* camera parameters */
  ASSOC_INT("USE_CAHV", useCAHV, 0, "")
  ASSOC_FLOAT_SCALED("BASELINE", baseline, 0.0, "distance between the cameras", 1.0/1000.0)  /* from [mm] to [m] */
  ASSOC_FLOAT_SCALED("TILT_PIVOT_OFFSET", tilt_pivot_offset, 0.0, "vert dist btwn optical axis and tilt axis", 1.0/1000.0)
  ASSOC_FLOAT_SCALED("CAMERA_OFFSET", camera_offset, 0.0, "horz dist btwn cam nodal pt and tilt axis", 1.0/1000.0)
  ASSOC_FLOAT("X_OFFSET", x_pivot_offset, 0.0, "offset btw wolrd origin and the hz pivot")
  ASSOC_FLOAT("Y_OFFSET", y_pivot_offset, 0.0, "")
  ASSOC_FLOAT("Z_OFFSET", z_pivot_offset, 0.0, "")
  ASSOC_FLOAT_SCALED("R_TOE_IN_0", toe_r, 0.0, "toe in for the right eye", 1.0/1000.0)
  ASSOC_FLOAT_SCALED("L_TOE_IN_0", toe_l, 0.0, "toe in for the left eye", 1.0/1000.0)
  ASSOC_FLOAT_SCALED("H_THETA_R_PIXEL", h_theta_Rpixel, 0.0, "field of view per pixel", 1.0/1000.0)
  ASSOC_FLOAT_SCALED("H_THETA_L_PIXEL", h_theta_Lpixel, 0.0, "", 1.0/1000.0)
  ASSOC_FLOAT_SCALED("V_THETA_R_PIXEL", v_theta_Rpixel, 0.0, "", 1.0/1000.0)
  ASSOC_FLOAT_SCALED("V_THETA_L_PIXEL", v_theta_Lpixel, 0.0, "", 1.0/1000.0)

  ASSOC_INT("OUT_WIDTH", out_width, 0, "desired image output size")
  ASSOC_INT("OUT_HEIGHT", out_height, 0, "")
  ASSOC_FLOAT("NEAR_UNIVERSE_RADIUS", near_universe_radius, 0.0, "radius of inner boundary of universe [m]")
  ASSOC_FLOAT("FAR_UNIVERSE_RADIUS", far_universe_radius, 0.0, "radius of outer boundary of universe [m]")
  //NOTE: UNIVERSE_RADIUS was an alias for FAR_UNIVERSE_RADIUS; now it is special-cased in the parser (but only for old-style config files)
  ASSOC_FLOAT("GROUND_PLANE_LEVEL", ground_plane, -1.0, "radius of outer boundary of universe [m]")
  ASSOC_FLOAT("SKY_BILLBOARD_ELEVATION", sky_billboard_elevation, 3.0, "Angle (deg.) above which to place everything on billboard")
  ASSOC_INT("SKY_BRIGHTNESS_THRESHOLD", sky_brightness_threshold, 0, "Intensity above which to sky dot on billboard")
  ASSOC_INT("RM_H_HALF_KERN", rm_h_half_kern, 0, "low conf pixel removal kernel half size")
  ASSOC_INT("RM_V_HALF_KERN", rm_v_half_kern, 0, "")
  ASSOC_INT("RM_MIN_MATCHES", rm_min_matches, 0, "min # of pxls to be matched to keep pxl")
  ASSOC_INT("RM_TRESHOLD", rm_treshold, 1, "rm_treshold > disp[n]-disp[m] pixels are not matching")
  ASSOC_FLOAT("SMR_TRESHOLD", smr_treshold, 0, "treshold for smooth_range function")
  ASSOC_INT("V_FILL_TRESHOLD", v_fill_treshold, 0, "treshold for the file_hole_vert function")
  ASSOC_INT("H_FILL_TRESHOLD", h_fill_treshold, 0, "treshold for the file_hole_vert function")
  ASSOC_INT("NFF_V_STEP", nff_v_step, 10, "")
  ASSOC_INT("NFF_H_STEP", nff_h_step, 10, "")
  ASSOC_INT("MOSAIC_V_STEP", mosaic_v_step, 25, "")
  ASSOC_INT("MOSAIC_H_STEP", mosaic_h_step, 25, "")
  ASSOC_FLOAT("MOSAIC_SPHERE_CENTER_X", mosaic_sphere_center_x, 0.0, "x coord of mosaic sphere center")
  ASSOC_FLOAT("MOSAIC_SPHERE_CENTER_Y", mosaic_sphere_center_y, 0.0, "y coord of mosaic sphere center")
  ASSOC_FLOAT("MOSAIC_SPHERE_CENTER_Z", mosaic_sphere_center_z, 0.0, "z coord of mosaic sphere center")
  ASSOC_INT("DRAW_MOSAIC_GROUND_PLANE", draw_mosaic_ground_plane, 0, "draw the ground plane for mosaics")
  ASSOC_INT("MOSAIC_IGNORE_INTENSITY", mosaic_ignore_intensity, 0, "ignore black pixels in mosaics")
  ASSOC_INT("NFF_MAX_JUMP", nff_max_jump, 0, "")
  ASSOC_INT("NFF_2D_MAP", nff_2d_map, 0, "")
  ASSOC_INT("VERBOSE", verbose, 1, "")
  ASSOC_FLOAT_SCALED("PAN_OFFSET", pan_offset, 0.0, "offset added to pan/tilt read in header", M_PI/180.0)
  ASSOC_FLOAT_SCALED("TILT_OFFSET", tilt_offset, 0.0, "", M_PI/180.0)
  ASSOC_FLOAT("ALTITUDE_RANGE", altitude_range, 1.0, "for the altitude texturing")
  ASSOC_FLOAT("ALTITUDE_OFFSET", altitude_offset, 0.0, "")
  ASSOC_INT("ALTITUDE_MODE", altitude_mode, 0, "0 limited 1 periodic")
  ASSOC_FLOAT("ALT_TOP_COLOR", alt_top_color, 120.0, "")
  ASSOC_FLOAT("ALT_BOTTOM_COLOR", alt_btm_color, 0, "")
  ASSOC_FLOAT("TEXTURE_CONTRAST", texture_cntrst, 1.0, "")
  ASSOC_FLOAT("X_DISP_CORRECTION", x_disp_corr, 0.0, "correct small/linear distortion")
  ASSOC_FLOAT("Y_DISP_CORRECTION", y_disp_corr, 0.0, "in disparity map")
  ASSOC_FLOAT("DISP_CORR_OFFSET", disp_corr_offset, 0.0, "")
  ASSOC_INT("MOSAIC", mosaic, 0, "mosaic'ing mode")
  ASSOC_INT("SM_DISP_M",smooth_disp_M, 19, "matrix size for disparity smoothing")
  ASSOC_INT("SM_DISP_N",smooth_disp_N, 19, "")
  ASSOC_INT("EXTEND_DISP_L",Lextend, 0, "# of pxl to extrapltd (L/R) the disp map")
  ASSOC_INT("EXTEND_DISP_R",Rextend, 0, "")
  ASSOC_INT("EXTEND_DISP_T",Textend, 0, "")
  ASSOC_INT("EXTEND_DISP_B",Bextend, 0, "")
  ASSOC_INT("OFFSET_DISP_T",Toffset, 0, "extrapolated pixel offset")
  ASSOC_INT("OFFSET_DISP_B",Boffset, 0, "")
  ASSOC_FLOAT("A2", lens_corr2, 0.0, "")
  ASSOC_FLOAT("A1", lens_corr1, 0.0, "")
  ASSOC_FLOAT("A0", lens_corr0, 0.0, "")
  ASSOC_FLOAT("MODEL_SCALE", range_scale, 1.0, "model scaling factor")
  ASSOC_FLOAT("IMP_AZ_OFFSET", imp_az_offset, 0.0, "offset btwn 0 motor count & cam x axis")
  ASSOC_FLOAT("IMP_CAN_Z_OFFSET", imp_can_z_offset, 0.0, "z offset btwn imp origin and el/az axis")
  ASSOC_FLOAT("X_IMP_OFFSET", x_imp_offset, 0.0, "offset between imp and lander frame")
  ASSOC_FLOAT("Y_IMP_OFFSET", y_imp_offset, 0.0, "")
  ASSOC_FLOAT("Z_IMP_OFFSET", z_imp_offset, 0.0, "")
  ASSOC_FLOAT("LOCAL_LEVEL_X", local_level_x, 0.0, "quaternion for rotating terrain")
  ASSOC_FLOAT("LOCAL_LEVEL_Y", local_level_y, 0.0, "into local level frame")
  ASSOC_FLOAT("LOCAL_LEVEL_Z", local_level_z, 0.0, "")
  ASSOC_FLOAT("LOCAL_LEVEL_W", local_level_w, 1.0, "")
  ASSOC_INT("FAR_FIELD_BILLBOARD", billboard_on, 1, "put the far field pixel on a billboard")
  ASSOC_INT("DO_SKY_BILLBOARD", sky_billboard, 0, "do place everything higher than a given elev. on billboard")
  ASSOC_FLOAT("OUT_MESH_SCALE", out_mesh_scale, 1.0, "scale factor for the output mesh")
  ASSOC_FLOAT("SUB_PXL_TRESHOLD", sub_pxl_treshold, 1.0, "set disp treshold limit for valid subpxl")
  ASSOC_FLOAT("MASK_LOW_CONTRAST_TRESHOLD",mask_low_contrast_treshold, 1.0, "low contrast mask treshold value")
  ASSOC_INT("H_TIE_PTS",h_tie_pts, 10, "number of tie pt for image alignment") 
  ASSOC_INT("V_TIE_PTS",v_tie_pts, 10, "")           
  ASSOC_FLOAT("XCORR_TRESHOLD", xcorr_treshold, 2.0, "")
  ASSOC_FLOAT("ALIGN.h11",alignMatrix.h11, 1.0, "homogenous matrix for linear image align")           
  ASSOC_FLOAT("ALIGN.h12",alignMatrix.h12, 0.0, "")
  ASSOC_FLOAT("ALIGN.h13",alignMatrix.h13, 0.0, "")
  ASSOC_FLOAT("ALIGN.h21",alignMatrix.h21, 0.0, "")
  ASSOC_FLOAT("ALIGN.h22",alignMatrix.h22, 1.0, "")
  ASSOC_FLOAT("ALIGN.h23",alignMatrix.h23, 0.0, "")
  ASSOC_FLOAT("ALIGN.h31",alignMatrix.h31, 0.0, "")
  ASSOC_FLOAT("ALIGN.h32",alignMatrix.h32, 0.0, "")
  ASSOC_FLOAT("ALIGN.h33",alignMatrix.h33, 1.0, "")
  ASSOC_FLOAT("RED_CHANEL_FACTOR",rFct, 1.0, "ppm images channel weight factor")
  ASSOC_FLOAT("GREEN_CHANEL_FACTOR",gFct, 1.0, "")
  ASSOC_FLOAT("BLUE_CHANEL_FACTOR",bFct, 1.0, "")
  ASSOC_FLOAT("SLOG_KERNEL_WIDTH",slogW, 0.0, "")
  ASSOC_INT("ALIGN_MARGIN_%_REJECT",align_margin, 10, "percentage of tie pts to reject")
  ASSOC_INT("REFERENCE_CAMERA",ref_cam, 0, "use the right camera as a reference")
  ASSOC_INT("MASTER_EYE",ref_eye, 0, "use the right eye as a reference")

  ASSOC_INT("TEXTURE_CASTING_TYPE", texture_casting_type, 0, "0 = fit between Imax and Imin")
  ASSOC_INT("MAX_GRAY_IN_TEXTURE", max_gray_in_texture, 4095, "")
  ASSOC_INT("MIN_GRAY_IN_TEXTURE", min_gray_in_texture, 0, "")
  ASSOC_INT("USE_MOTOR_COUNT", use_motor_count, 0, "use the motor count instead of the MIPL value")

  ASSOC_FLOAT("AMBIENT_RED",ambiColorRed, 0.2, "VRML / IV red ambiant color")
  ASSOC_FLOAT("AMBIENT_GREEN",ambiColorGreen, 0.2, "VRML / IV green ambiant color")
  ASSOC_FLOAT("AMBIENT_BLUE",ambiColorBlue, 0.2, "VRML / IV blue ambiant  color")
  ASSOC_FLOAT("DIFFUSE_RED",diffColorRed, 0.8, "VRML / IV red diffuse color")
  ASSOC_FLOAT("DIFFUSE_GREEN",diffColorGreen, 0.8, "VRML / IV green diffuse color")
  ASSOC_FLOAT("DIFFUSE_BLUE",diffColorBlue, 0.8, "VRML / IV blue diffuse color")
  ASSOC_FLOAT("SPECULAR_RED",specColorRed, 0.0, "VRML / IV red specular color")
  ASSOC_FLOAT("SPECULAR_GREEN",specColorGreen, 0.0, "VRML / IV green specular color")
  ASSOC_FLOAT("SPECULAR_BLUE",specColorBlue, 0.0, "VRML / IV blue specular color")
  ASSOC_FLOAT("EMISSIVE_RED",emisColorRed, 1.0, "VRML / IV red emissive color")
  ASSOC_FLOAT("EMISSIVE_GREEN",emisColorGreen, 1.0, "VRML / IV green emissive color")
  ASSOC_FLOAT("EMISSIVE_BLUE",emisColorBlue, 1.0, "VRML / IV blue emissive color")
  ASSOC_FLOAT("SHININESS",shininess, 0.2, "VRML / IV model shininess")
  ASSOC_FLOAT("TRANSPARENCY",transparency, 0.0, "VRML / IV model transarency")
  ASSOC_FLOAT("CREASE_ANGLE",creaseAngle, 1.5, "VRML / IV crease angle")
  ASSOC_INT("SHAPE_TYPE_SOLID",shapeType_solid, 1, "VRML / IV back face culling")

  ASSOC_DOUBLE("MESH_TOLERANCE",mesh_tolerance, 0.001, "tolerance of mesh")
  ASSOC_INT("MAX_TRIANGLES",max_triangles, 500000, "maximum number of triangles in the mesh")
  ASSOC_INT("WRITE_TEXTURE_SWITCH", write_texture_switch, 0, "write the vrml texture switch by default T.rgb, S.rgb, M.rgb, A.rgb")

  ASSOC_FLOAT("DEM_SPACING", dem_spacing, 3.0, "The USGS standard is 3 arc secs or 30 meters")
  ASSOC_FLOAT("DEM_PLANET_RADIUS", dem_planet_radius, MOLA_PEDR_EQUATORIAL_RADIUS, "Nominal Mars polar radius according to the IAU 2000 standard")
  ASSOC_INT("ENVI_DEM_DATA_TYPE", ENVI_dem_data_type, ENVI_float_32bit, "")

#undef ASSOC_INT
#undef ASSOC_INT_SCALED
#undef ASSOC_FLOAT
#undef ASSOC_FLOAT_SCALED
#undef ASSOC_DOUBLE
#undef ASSOC_DOUBLE_SCALED
#undef ASSOC_TO_DO

}

/************************************************************************/
/*	             initialize Header Structure            		*/
/************************************************************************/

void init_dft_struct(DFT_F *dft, TO_DO *todo) {
  po::options_description desc;
  std::vector<AugmentingDescription> adesc;
  po::variables_map vm;
  int argc = 1;
  char* argv[1];
  argv[0] = "dummyprogname";
  associate_dft_struct(dft, todo, &desc, &adesc);
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  scale_dft_struct(&desc, &adesc, &vm);
}

/************************************************************************/
/*									*/
/*		          read stereo.default				*/
/*									*/
/************************************************************************/

// Determine whether the given character is a space character.
inline bool
is_space_char(int c) {
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

// Read from stream until (but not including) the next non-space character.
inline void
ignorespace(std::istream& s) {
  int c;
  if(s.eof())
    return;
  while((c = s.get()) != EOF && is_space_char(c));
  if(c != EOF)
    s.unget();
}

// Read from stream until (but not including) the beginning of the next line.
inline void
ignoreline(std::istream& s) {
  int c;
  if(s.eof())
    return;
  while((c = s.get()) != EOF && c != '\n');
}

// Read from stream until (but not including) the next space character. Ignores space characters at beginning.
inline void
getword(std::istream& s, std::string& str) {
  int c;
  str.clear();
  ignorespace(s);
  if(s.eof())
    return;
  while((c = s.get()) != EOF && !is_space_char(c))
    str.push_back(c);
  if(c != EOF)
    s.unget();
}

// Read default file.
void
read_default_file(DFT_F *dft, TO_DO *execute, const char *filename) {
  po::options_description desc;
  std::vector<AugmentingDescription> adesc;
  po::variables_map vm;
  std::ifstream fp;
  associate_dft_struct(dft, execute, &desc, &adesc);
  fp.open(filename);
  if(fp.bad()) {
    std::cerr << "Error: cannot open stereo default file." << std::endl;
    exit(EXIT_FAILURE);
  }
#ifdef READ_OLD_CONFIG_FILE
  if((fp.get() == 'S') && (!fp.eof() && fp.get() == 'D') && (!fp.eof() && fp.get() == 'F')) {
    std::cout << "Reading old-style stereo default file." << std::endl;
    std::string name, value, line;
    int c;
    //NOTE: Unlike the command-line options parsing facility, the config-file parsing facility does not allow custom parsers, so we need to parse and reformat each line ourselves and then pass the reformatted lines to po::parse_config_file() separately as if they were all separate config files.
    while(!fp.eof()) {
      ignorespace(fp);
      if(!fp.eof() && (c = fp.peek()) != '#') {
        std::istringstream ss; //NOTE: cannot move this up with other variable declarations because then calling store(parse_config_file()) multiple times does not work as expected
        getword(fp, name);
        if(name == "END")
          break;
        // special case for UNIVERSE_RADIUS, which is an alias for FAR_UNIVERSE_RADIUS
        if(name == "UNIVERSE_RADIUS")
          name = "FAR_UNIVERSE_RADIUS";
        getword(fp, value);
        line = name.append(" = ").append(value);
        ss.str(line);
        //NOTE: Unlike the command-line options parsing facility, the config-file parsing facility does not allow us to specify that it should ignore unknown options, so this call chokes on unknown config options.
        po::store(po::parse_config_file(ss, desc), vm);
      }
      ignoreline(fp);
    }
  }
  else {
    std::cout << "Reading new-style stereo default file." << std::endl;
    fp.seekg(0); // rewind
#endif
    //NOTE: Unlike the command-line options parsing facility, the config-file parsing facility does not allow us to specify that it should ignore unknown options, so this call chokes on unknown config options.
    po::store(po::parse_config_file(fp, desc), vm);
#ifdef READ_OLD_CONFIG_FILE
  }
#endif
  po::notify(vm);
  scale_dft_struct(&desc, &adesc, &vm);
  if(dft->verbose) {
    printf(" *************************************************************\n");
    printf("Stereo Default File loaded successfully\n");
  }
  fp.close();
}

/************************************************************************/
/*									*/
/*		          write stereo.default				*/
/*									*/
/************************************************************************/

// Write config file. Analogous to po::parse_config_file().
//NOTE: adesc is to get around the fact that we cannot get the store-to pointers back out of the po::value's inside the po::options_description
void write_config_file(std::basic_ostream<char>& s, po::options_description& desc, std::vector<AugmentingDescription>& adesc) {
  OptionType type;
  std::vector<AugmentingDescription>::iterator pi;
  std::string name, value, description;
  void* ptr;
  bool first = true;
#ifdef WRITE_OLD_CONFIG_FILE
  s << "SDF" << std::endl;
#endif
  //NOTE: instead of iterating over adesc, we could ask desc for the po::option_description's; however, the interface for getting this information changed between boost versions 1.32.0 and 1.33.0 (there is no other 1.32.x--these are consecutive releases). Version 1.32.0 (the first release of boost with program_options) has a function keys() that returns a std::set of option names (in alphabetical order, not in the original option order), which you can then find(name). Versions 1.33.0 and up instead have a function options() that directly returns a vector of (boost::shared_ptr's to) po::option_description's (presumably in the original option order given above), although it also supports find(name, approx). Since we're already using adesc, we might as well take advantage of it to (partially--note that find() has changed) avoid this unstable interface and to always get the options in the original order.
  for(pi = adesc.begin(); pi != adesc.end(); pi++) {
#if BOOST_VERSION == 103200
    po::option_description d = desc.find((*pi).name);
#else
    po::option_description d = desc.find((*pi).name, false);
#endif
    name = d.long_name();
    if(name.empty()) {
      std::string origname((*pi).name);
      std::cerr << "Error: Option " << origname << " must have a long name for a config file." << std::endl;
      exit(EXIT_FAILURE);
    }
    description = d.description();
    type = option_type(d);
    //NOTE: unfortunately, we cannot get the store-to pointer back out of d.semantic() (this is a "po::value()"/po::signed_value()), so we use adesc
    ptr = (*pi).data;
    // Finally, we output to the file
    s << std::endl;
    if(!description.empty())
      s << "# " << description << std::endl;
    s << name;
#ifdef WRITE_OLD_CONFIG_FILE
    s << "\t";
#else
    s << " = ";
#endif
    switch(type) {
    case OPTION_TYPE_INT:
      s << *((int*)ptr) << std::endl;
      break;
    case OPTION_TYPE_FLOAT:
      s << fixed << *((float*)ptr) << std::endl;
      break;
    case OPTION_TYPE_DOUBLE:
      s << fixed << *((double*)ptr) << std::endl;
      break;
    default:
      std::cerr << "Unexpected type!" << std::endl;
      break;
    }
    first = false;
  }
#ifdef WRITE_OLD_CONFIG_FILE
  if(!first)
    s << std::endl;
  s << "END" << std::endl;
#endif
}

void
write_default_file(DFT_F *dft, TO_DO *execute, const char *filename) {
  po::options_description desc;
  std::vector<AugmentingDescription> adesc;
  std::ofstream fp;
  DFT_F dftc = *dft; // copy dft so that we can unscale it
  fp.open(filename);
  if(fp.bad()) {
    std::cerr << "Error: cannot open stereo default file." << std::endl;
    exit(EXIT_FAILURE);
  }
  associate_dft_struct(&dftc, execute, &desc, &adesc);
  unscale_dft_struct(&desc, &adesc);
  write_config_file(fp, desc, adesc);
  if(dft->verbose) {
    printf(" *************************************************************\n");
    printf("Stereo Default File written successfully\n");
  }
  fp.close();
}

/************************************************************************/
/*	             initialize Header Structure            		*/
/************************************************************************/

void init_header_struct(DFT_F *dft, 
                        F_HD *hd, 
                        char *cmd_name, 
                        char *input_file, 
                        char *output_file) {
  hd->in_file = NULL;
  hd->in_file2 = NULL;
  hd->out_file = NULL;
  hd->cmd_name = NULL;
  hd->extention = NULL;
  hd->type = NULL;

  hd->in_file = (char *)malloc (NAME_LENGTH * sizeof(char));
  strcpy (hd->in_file, input_file);

  hd->out_file = (char *)malloc (NAME_LENGTH * sizeof(char));
  strcpy (hd->out_file, output_file);

  hd->cmd_name = (char *)malloc (NAME_LENGTH * sizeof(char));
  strcpy (hd->cmd_name, cmd_name);
  
  hd->width = 0;		/* Image width */
  hd->height = 0;		/* Image height */
  hd->total_height = 0;		/* Height of all the buffer */
  hd->max_gray = 0;		/* Max gray level in the pgm image */
  hd->extention = NULL;		/* type file Extention */
  hd->h_theta_pixel = 0.0;	/* [rad/pixel] horizontaly */
  hd->v_theta_pixel = 0.0; 	/* [rad/pixel] vertically */
  hd->pan = 0.0;		/* pan angle */
  hd->tilt = 0.0;		/* tilt angle */
  hd->hcf = 1.0;		/* horizontal compression factor */
  hd->vcf = 1.0;		/* vertical compression factor */
  hd->h_fov = 0.0;		/* horizontal field of view */
  hd->v_fov = 0.0;		/* vertical field of view */

  if(dft->verbose && 0)
    printf("Header structure initialized succesfully\n");

  return;
} /* init_header_struct */


/*******/
/* END */
/*******/

/*
 * This file is part of the CR2RES Pipeline
 * Copyright (C) 2002,2003 European Southern Observatory
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*-----------------------------------------------------------------------------
                                Includes
 -----------------------------------------------------------------------------*/

#include <math.h>
#include <cpl.h>
#include "hdrl.h"

#include "cr2res_utils.h"
#include "cr2res_pfits.h"
#include "cr2res_dfs.h"
#include "cr2res_io.h"
#include "cr2res_bpm.h"
#include "cr2res_calib.h"

/*-----------------------------------------------------------------------------
                                Define
 -----------------------------------------------------------------------------*/

#define RECIPE_STRING "cr2res_cal_dark"

/*-----------------------------------------------------------------------------
                             Plugin registration
 -----------------------------------------------------------------------------*/

int cpl_plugin_get_info(cpl_pluginlist * list);

/*-----------------------------------------------------------------------------
                            Private function prototypes
 -----------------------------------------------------------------------------*/

static int cr2res_cal_dark_compare(
        const cpl_frame   *   frame1,
        const cpl_frame   *   frame2) ;

static int cr2res_cal_dark_create(cpl_plugin *);
static int cr2res_cal_dark_exec(cpl_plugin *);
static int cr2res_cal_dark_destroy(cpl_plugin *);
static int cr2res_cal_dark(cpl_frameset *, const cpl_parameterlist *);

/*-----------------------------------------------------------------------------
                            Static variables
 -----------------------------------------------------------------------------*/

static char cr2res_cal_dark_description[] =
"CRIRES+ dark recipe\n"
"The files listed in the Set Of Frames (sof-file) must be tagged:\n"
"raw-file.fits " CR2RES_DARK_RAW "\n"
"detlin.fits " CR2RES_DETLIN_BPM_PROCATG "\n"
" The recipe produces the following products for each different DIT found:\n"
"cr2res_cal_dark_master_DIT.fits " CR2RES_MASTER_DARK_PROCATG "\n"
"cr2res_cal_dark_bpm_DIT.fits " CR2RES_DARK_BPM_PROCATG "\n"
"\n";

/*-----------------------------------------------------------------------------
                                Function code
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Build the list of available plugins, for this module. 
  @param    list    the plugin list
  @return   0 if everything is ok, 1 otherwise
  @note     Only this function is exported

  Create the recipe instance and make it available to the application using the 
  interface. 
 */
/*----------------------------------------------------------------------------*/
int cpl_plugin_get_info(cpl_pluginlist * list)
{
    cpl_recipe  *   recipe = cpl_calloc(1, sizeof *recipe );
    cpl_plugin  *   plugin = &recipe->interface;

    if (cpl_plugin_init(plugin,
                    CPL_PLUGIN_API,
                    CR2RES_BINARY_VERSION,
                    CPL_PLUGIN_TYPE_RECIPE,
                    "cr2res_cal_dark",
                    "Dark recipe",
                    cr2res_cal_dark_description,
                    "Thomas Marquart, Yves Jung",
                    PACKAGE_BUGREPORT,
                    cr2res_get_license(),
                    cr2res_cal_dark_create,
                    cr2res_cal_dark_exec,
                    cr2res_cal_dark_destroy)) {    
        cpl_msg_error(cpl_func, "Plugin initialization failed");
        (void)cpl_error_set_where(cpl_func);                          
        return 1;                                               
    }                                                    

    if (cpl_pluginlist_append(list, plugin)) {                 
        cpl_msg_error(cpl_func, "Error adding plugin to list");
        (void)cpl_error_set_where(cpl_func);                         
        return 1;                                              
    }                                                          
    
    return 0;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Setup the recipe options    
  @param    plugin  the plugin
  @return   0 if everything is ok

  Defining the command-line/configuration parameters for the recipe.
 */
/*----------------------------------------------------------------------------*/
static int cr2res_cal_dark_create(cpl_plugin * plugin)
{
    cpl_recipe          *   recipe ;
    cpl_parameter       *   p ;
    cpl_parameterlist   *   collapse_par ;
    hdrl_parameter      *   sigclip_def ;
    hdrl_parameter      *   minmax_def ;

    /* Check that the plugin is part of a valid recipe */
    if (cpl_plugin_get_type(plugin) == CPL_PLUGIN_TYPE_RECIPE)
        recipe = (cpl_recipe *)plugin;
    else
        return -1;

    /* Create the parameters list in the cpl_recipe object */
    recipe->parameters = cpl_parameterlist_new();

    /* Fill the parameters list */

    /* --detector */
    p = cpl_parameter_new_value("cr2res.cr2res_cal_dark.detector",
            CPL_TYPE_INT, "Only reduce the specified detector",
            "cr2res.cr2res_cal_dark", 0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "detector");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    /* --bpm_low */
    p = cpl_parameter_new_value("cr2res_cal_dark.bpm_low", CPL_TYPE_DOUBLE,
            "Low Threshold for the BPM", "cr2res_cal_dark", 0.5);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "bpm_low");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    /* --bpm_high */
    p = cpl_parameter_new_value("cr2res_cal_dark.bpm_high", CPL_TYPE_DOUBLE,
            "High Threshold for the BPM", "cr2res_cal_dark", 2.0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "bpm_high");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    /* --bpm_lines_ratio */
    p = cpl_parameter_new_value("cr2res_cal_dark.bpm_lines_ratio", 
            CPL_TYPE_DOUBLE, "Maximum ratio of bad pixels per line", 
            "cr2res_cal_dark", 0.5);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "bpm_lines_ratio");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    /* --gain */
    p = cpl_parameter_new_value("cr2res_cal_dark.gain", CPL_TYPE_DOUBLE,
       "Gain in [e- / ADU]", "cr2res_cal_dark", 2.5);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "gain");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    /* Collapsing related parameters */
    sigclip_def = hdrl_collapse_sigclip_parameter_create(3., 3., 5);
    minmax_def = hdrl_collapse_minmax_parameter_create(1., 1.);
    collapse_par = hdrl_collapse_parameter_create_parlist("cr2res_cal_dark", 
            "", "MEDIAN", sigclip_def, minmax_def) ;
    hdrl_parameter_delete(sigclip_def);
    hdrl_parameter_delete(minmax_def);
    for (p = cpl_parameterlist_get_first(collapse_par) ;
            p != NULL; p = cpl_parameterlist_get_next(collapse_par))
        cpl_parameterlist_append(recipe->parameters,cpl_parameter_duplicate(p));
    cpl_parameterlist_delete(collapse_par);

    return 0;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Execute the plugin instance given by the interface
  @param    plugin  the plugin
  @return   0 if everything is ok
 */
/*----------------------------------------------------------------------------*/
static int cr2res_cal_dark_exec(cpl_plugin * plugin)
{
    cpl_recipe  *recipe;

    /* Get the recipe out of the plugin */
    if (cpl_plugin_get_type(plugin) == CPL_PLUGIN_TYPE_RECIPE)
        recipe = (cpl_recipe *)plugin;
    else return -1;

    return cr2res_cal_dark(recipe->frames, recipe->parameters);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Destroy what has been created by the 'create' function
  @param    plugin  the plugin
  @return   0 if everything is ok
 */
/*----------------------------------------------------------------------------*/
static int cr2res_cal_dark_destroy(cpl_plugin * plugin)
{
    cpl_recipe *recipe;

    /* Get the recipe out of the plugin */
    if (cpl_plugin_get_type(plugin) == CPL_PLUGIN_TYPE_RECIPE)
        recipe = (cpl_recipe *)plugin;
    else return -1 ;

    cpl_parameterlist_delete(recipe->parameters);
    return 0 ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Interpret the command line options and execute the data processing
  @param    frameset   the frames list
  @param    parlist    the parameters list
  @return   0 if everything is ok
 */
/*----------------------------------------------------------------------------*/
static int cr2res_cal_dark(
        cpl_frameset            *   frameset,
        const cpl_parameterlist *   parlist)
{
    const cpl_parameter *   par ;
    int                     reduce_det ;
    double                  gain, dit, bpm_low, bpm_high, bpm_lines_ratio ;
    hdrl_parameter      *   collapse_params ;
    cpl_frameset        *   rawframes ;
    cpl_frameset        *   raw_one ;
    cpl_size            *   labels ;
    cpl_size                nlabels ;
    const char          *   detlin ;
    cpl_propertylist    *   plist ;
    
    hdrl_image          *   master_darks[CR2RES_NB_DETECTORS] ;
    hdrl_image          *   bpms[CR2RES_NB_DETECTORS] ;
    cpl_propertylist    *   ext_plist[CR2RES_NB_DETECTORS] ;

    hdrl_imagelist      *   dark_cube ;
    cpl_image           *   my_bpm ;
    const char          *   fname ;
    cpl_image           *   ima_data ;
    cpl_image           *   ima_err ;
    hdrl_image          *   hdrl_ima ;
    hdrl_image          *   master;
    cpl_image           *   contrib_map;
    char                *   filename ;
    int                     nb_frames, i, l, ext ;

    /* RETRIEVE INPUT PARAMETERS */
    /* --detector */
    par = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_cal_dark.detector");
    reduce_det = cpl_parameter_get_int(par);

    /* --bpm_low */
    par = cpl_parameterlist_find_const(parlist, "cr2res_cal_dark.bpm_low");
    bpm_low = cpl_parameter_get_double(par);

    /* --bpm_high */
    par = cpl_parameterlist_find_const(parlist, "cr2res_cal_dark.bpm_high");
    bpm_high = cpl_parameter_get_double(par);

    /* --bpm_lines_ratio */
    par = cpl_parameterlist_find_const(parlist,
            "cr2res_cal_dark.bpm_lines_ratio");
    bpm_lines_ratio = cpl_parameter_get_double(par);

    /* --gain */
    par = cpl_parameterlist_find_const(parlist, "cr2res_cal_dark.gain");
    gain = cpl_parameter_get_double(par);
    /* Collapse parameters */
    collapse_params = hdrl_collapse_parameter_parse_parlist(parlist,
            "cr2res_cal_dark") ;
   
    /* Identify the RAW and CALIB frames in the input frameset */
    if (cr2res_dfs_set_groups(frameset)) {
        hdrl_parameter_destroy(collapse_params) ;
        cpl_msg_error(__func__, "Cannot identify RAW and CALIB frames") ;
        cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
        return -1 ;
    }
    
    /* Retrieve calibration data */
    detlin = cr2res_extract_filename(frameset, CR2RES_DETLIN_COEFFS_PROCATG) ;

    /* Extract RAW frames */
    rawframes = cr2res_extract_frameset(frameset, CR2RES_DARK_RAW) ;
    if (rawframes==NULL || cpl_frameset_get_size(rawframes) <= 0) {
        hdrl_parameter_destroy(collapse_params) ;
        cpl_msg_error(__func__, "Cannot find any RAW file") ;
        cpl_error_set(__func__, CPL_ERROR_DATA_NOT_FOUND) ;
        return -1 ;
    }

    /* Labelise the raw frames with the DIT */
    if ((labels = cpl_frameset_labelise(rawframes, cr2res_cal_dark_compare,
                &nlabels)) == NULL) {
        cpl_msg_error(__func__, "Cannot labelise input frames") ;
        cpl_frameset_delete(rawframes) ;
        hdrl_parameter_destroy(collapse_params) ;
        cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
        return -1 ;
    }

    /* Loop on the settings */
    for (l=0 ; l<(int)nlabels ; l++) {
        /* Get the frames for the current setting */
        raw_one = cpl_frameset_extract(rawframes, labels, (cpl_size)l) ;
        nb_frames = cpl_frameset_get_size(raw_one) ;

        /* Get the current DIT */
        plist = cpl_propertylist_load(cpl_frame_get_filename(
                    cpl_frameset_get_position(raw_one, 0)), 0) ;
        dit = cr2res_pfits_get_dit(plist) ;
        cpl_propertylist_delete(plist) ;

        cpl_msg_info(__func__, "Process DIT %g", dit) ;
        cpl_msg_indent_more() ;

        /* Loop on the extensions */
        for (ext=1 ; ext<=CR2RES_NB_DETECTORS ; ext++) {
            cpl_msg_info(__func__, "Process Detector nb %i", ext) ;
            cpl_msg_indent_more() ;

            /* Initialise */
            master_darks[ext-1] = NULL ;
            bpms[ext-1] = NULL ;
            ext_plist[ext-1] = NULL ;

            /* Compute only one detector */
            if (reduce_det != 0 && ext != reduce_det) continue ;

            /* Loop on the frames */
            dark_cube = hdrl_imagelist_new();
            for (i=0; i<nb_frames ; i++) {
                /* Identify current file */
                fname=cpl_frame_get_filename(
                        cpl_frameset_get_position(raw_one, i)) ; 
                cpl_msg_info(__func__, "Load Image from File %s / Detector %i", 
                        fname, ext) ;
                
                /* Load the image */
                if ((ima_data = cpl_image_load(fname, CPL_TYPE_FLOAT, 0,
                                ext)) == NULL) {
                    cpl_msg_error(__func__, 
                            "Cannot load image from File %s / Detector %d", 
                            fname, ext) ;
                    cpl_error_set(__func__, CPL_ERROR_DATA_NOT_FOUND) ;
                    cpl_msg_indent_less() ;
                    cpl_msg_indent_less() ;
                    cpl_frameset_delete(rawframes) ;
                    hdrl_parameter_destroy(collapse_params) ;
                    cpl_free(labels);
                    cpl_frameset_delete(raw_one) ;
                    hdrl_imagelist_delete(dark_cube) ;
                    return -1 ;
                }

                /* Create the noise image */
                cpl_msg_info(__func__, "Create the associated Noise image");
                if (cr2res_detector_shotnoise_model(ima_data, gain, 10.,
                            &ima_err) != CPL_ERROR_NONE) {
                    cpl_free(labels);
                    cpl_msg_error(__func__, "Cannot create the Noise image") ;
                    cpl_error_set(__func__, CPL_ERROR_DATA_NOT_FOUND) ;
                    cpl_msg_indent_less() ;
                    cpl_msg_indent_less() ;
                    cpl_frameset_delete(rawframes) ;
                    hdrl_parameter_destroy(collapse_params) ;
                    cpl_free(labels);
                    cpl_frameset_delete(raw_one) ;
                    hdrl_imagelist_delete(dark_cube) ;
                    cpl_image_delete(ima_data); 
                    return -1 ;
                }

                /* Store Data and Error together in an hdrl image */
                hdrl_ima = hdrl_image_create(ima_data, ima_err);
                cpl_image_delete(ima_data);
                cpl_image_delete(ima_err);
                
                /* Store the hdrl image in the dark_cube */
                hdrl_imagelist_set(dark_cube, hdrl_ima, i);
            }

            /* Get the proper collapsing function and do frames combination */
            if (hdrl_imagelist_collapse(dark_cube, collapse_params,
                    &(master_darks[ext-1]), &contrib_map) != CPL_ERROR_NONE) {
                cpl_msg_warning(__func__, "Cannot collapse Detector %d", ext) ;
                master_darks[ext-1] = NULL ;
                contrib_map = NULL ;
            }
            cpl_image_delete(contrib_map);
            hdrl_imagelist_delete(dark_cube);
       
            /* Compute BPM from the MASTER dark */
            if (master_darks[ext-1] != NULL) {
                if ((my_bpm = cr2res_compute_bpm(
                                hdrl_image_get_image(master_darks[ext-1]),
                                bpm_low, bpm_high, bpm_lines_ratio, 0))==NULL) {
                    cpl_msg_warning(__func__, "Cannot create bad pixels map") ;
                } else {
                    bpms[ext-1] = hdrl_image_create(my_bpm, NULL) ;
                    cpl_image_delete(my_bpm) ;
                }
            }

            /* QCs */
            if (master_darks[ext-1] != NULL) {
                ext_plist[ext-1] = NULL ;

            }
            cpl_msg_indent_less() ;
        }

        /* Save the results */
        /* MASTER DARK */
        filename = cpl_sprintf("%s_dit_%g_master.fits", RECIPE_STRING, dit); 
        if (cr2res_io_save_MASTER_DARK(filename, raw_one, parlist, master_darks,
                    NULL, ext_plist, CR2RES_MASTER_DARK_PROCATG, 
                    RECIPE_STRING) != 0) {
            cpl_frameset_delete(rawframes) ;
            cpl_frameset_delete(raw_one) ;
            for (ext=1 ; ext<=CR2RES_NB_DETECTORS ; ext++) {
                if (bpms[ext-1] != NULL) 
                    hdrl_image_delete(bpms[ext-1]);
                if (master_darks[ext-1] != NULL) 
                    hdrl_image_delete(master_darks[ext-1]);
                if (ext_plist[ext-1] != NULL) 
                    cpl_propertylist_delete(ext_plist[ext-1]);
            }
            cpl_free(labels);
            hdrl_parameter_destroy(collapse_params) ;
            cpl_free(filename) ;
            cpl_msg_error(__func__, "Cannot save the MASTER DARK") ;
            cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
            cpl_msg_indent_less() ;
            return -1 ;

        }
        cpl_free(filename) ;

        /* BPM */
        filename = cpl_sprintf("%s_dit_%g_bpm.fits", RECIPE_STRING, dit); 
        if (cr2res_io_save_BPM(filename, raw_one, parlist, bpms, NULL,
                    ext_plist, CR2RES_DARK_BPM_PROCATG, RECIPE_STRING) != 0) {
            cpl_frameset_delete(rawframes) ;
            cpl_frameset_delete(raw_one) ;
            for (ext=1 ; ext<=CR2RES_NB_DETECTORS ; ext++) {
                if (bpms[ext-1] != NULL) 
                    hdrl_image_delete(bpms[ext-1]);
                if (master_darks[ext-1] != NULL) 
                    hdrl_image_delete(master_darks[ext-1]);
                if (ext_plist[ext-1] != NULL) 
                    cpl_propertylist_delete(ext_plist[ext-1]);
            }
            cpl_free(labels);
            hdrl_parameter_destroy(collapse_params) ;
            cpl_free(filename) ;
            cpl_msg_error(__func__, "Cannot save the BPM") ;
            cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
            cpl_msg_indent_less() ;
            return -1 ;

        }
        cpl_free(filename) ;

        /* Free */
        cpl_frameset_delete(raw_one) ;
        for (ext=1 ; ext<=CR2RES_NB_DETECTORS ; ext++) {
            if (bpms[ext-1] != NULL) 
                hdrl_image_delete(bpms[ext-1]);
            if (master_darks[ext-1] != NULL) 
                hdrl_image_delete(master_darks[ext-1]);
            if (ext_plist[ext-1] != NULL) 
                cpl_propertylist_delete(ext_plist[ext-1]);
        }
        cpl_msg_indent_less() ;
    }
    cpl_free(labels);
    hdrl_parameter_delete(collapse_params);
    cpl_frameset_delete(rawframes) ;

    return (int)cpl_error_get_code();
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Comparison function to identify different settings
  @param    frame1  first frame
  @param    frame2  second frame
  @return   0 if frame1!=frame2, 1 if frame1==frame2, -1 in error case
 */
/*----------------------------------------------------------------------------*/
static int cr2res_cal_dark_compare(
        const cpl_frame   *   frame1,
        const cpl_frame   *   frame2)
{
    int                     comparison ;
    cpl_propertylist    *   plist1 ;
    cpl_propertylist    *   plist2 ;
    double                  dval1, dval2 ;

    /* Test entries */
    if (frame1==NULL || frame2==NULL) return -1 ;

    /* Get property lists */
    if ((plist1=cpl_propertylist_load(cpl_frame_get_filename(frame1),0))==NULL){
        cpl_msg_error(__func__, "getting header from reference frame");
        return -1 ;
    }
    if ((plist2=cpl_propertylist_load(cpl_frame_get_filename(frame2),0))==NULL){
        cpl_msg_error(__func__, "getting header from reference frame");
        cpl_propertylist_delete(plist1) ;
        return -1 ;
    }

    /* Test status */
    if (cpl_error_get_code()) {
        cpl_propertylist_delete(plist1) ;
        cpl_propertylist_delete(plist2) ;
        return -1 ;
    }

    comparison = 1 ;

    /* Compare the DIT used */
    dval1 = cr2res_pfits_get_dit(plist1) ;
    dval2 = cr2res_pfits_get_dit(plist2) ;
    if (cpl_error_get_code()) {
        cpl_msg_error(__func__, "Cannot get the DIT");
        cpl_propertylist_delete(plist1) ;
        cpl_propertylist_delete(plist2) ;
        return -1 ;
    }
    if (fabs(dval1-dval2) > 1e-3) comparison = 0 ;

    cpl_propertylist_delete(plist1) ;
    cpl_propertylist_delete(plist2) ;
    return comparison ;
}



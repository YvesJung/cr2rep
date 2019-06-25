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

#include <cpl.h>

#include "cr2res_utils.h"
#include "cr2res_pol.h"
#include "cr2res_nodding.h"
#include "cr2res_calib.h"
#include "cr2res_pfits.h"
#include "cr2res_dfs.h"
#include "cr2res_bpm.h"
#include "cr2res_trace.h"
#include "cr2res_extract.h"
#include "cr2res_io.h"
#include "cr2res_qc.h"

/*-----------------------------------------------------------------------------
                                Define
 -----------------------------------------------------------------------------*/

#define RECIPE_STRING "cr2res_obs_pol"

/*-----------------------------------------------------------------------------
                             Plugin registration
 -----------------------------------------------------------------------------*/

int cpl_plugin_get_info(cpl_pluginlist * list);

/*-----------------------------------------------------------------------------
                            Private function prototypes
 -----------------------------------------------------------------------------*/

static int cr2res_obs_pol_check_inputs_validity(
        const cpl_frameset  *   rawframes) ;
static int cr2res_obs_pol_reduce(
        const cpl_frameset  *   rawframes,
        const cpl_frameset  *   raw_flat_frames,
        const cpl_frame     *   trace_wave_frame,
        const cpl_frame     *   detlin_frame,
        const cpl_frame     *   master_dark_frame,
        const cpl_frame     *   master_flat_frame,
        const cpl_frame     *   bpm_frame,
        int                     calib_cosmics_corr,
        int                     extract_oversample,
        int                     extract_swath_width,
        int                     extract_height,
        double                  extract_smooth,
        int                     reduce_det,
        cpl_table           **  pol_spec_a,
        cpl_table           **  pol_spec_b,
        cpl_propertylist    **  ext_plista,
        cpl_propertylist    **  ext_plistb) ;
static int cr2res_obs_pol_reduce_one(
        const cpl_frameset  *   rawframes,
        const cpl_frameset  *   raw_flat_frames,
        const cpl_frame     *   trace_wave_frame,
        const cpl_frame     *   detlin_frame,
        const cpl_frame     *   master_dark_frame,
        const cpl_frame     *   master_flat_frame,
        const cpl_frame     *   bpm_frame,
        int                     calib_cosmics_corr,
        int                     extract_oversample,
        int                     extract_swath_width,
        int                     extract_height,
        double                  extract_smooth,
        int                     reduce_det,
        cpl_table           **  pol_spec,
        cpl_propertylist    **  ext_plist) ;
static int cr2res_obs_pol_create(cpl_plugin *);
static int cr2res_obs_pol_exec(cpl_plugin *);
static int cr2res_obs_pol_destroy(cpl_plugin *);
static int cr2res_obs_pol(cpl_frameset *, const cpl_parameterlist *);

/*-----------------------------------------------------------------------------
                            Static variables
 -----------------------------------------------------------------------------*/

static char cr2res_obs_pol_description[] =
"CRIRES+ Polarimetry Observation recipe\n"
"The files listed in the Set Of Frames (sof-file) must be tagged:\n"
"raw-file.fits " CR2RES_OBS_POL_RAW"\n"
"trace_wave.fits " CR2RES_FLAT_TRACE_WAVE_PROCATG "\n"
"detlin.fits " CR2RES_DETLIN_COEFFS_PROCATG "\n"
"master_dark.fits " CR2RES_MASTER_DARK_PROCATG "\n"
"master_flat.fits " CR2RES_FLAT_MASTER_FLAT_PROCATG "\n"
"bpm.fits " CR2RES_FLAT_BPM_PROCATG "\n"
" The recipe produces the following products:\n"
"cr2res_obs_pol_specA.fits " CR2RES_OBS_POL_SPECA_PROCATG "\n"
"cr2res_obs_pol_specB.fits " CR2RES_OBS_POL_SPECB_PROCATG "\n"
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
                    "cr2res_obs_pol",
                    "Polarimetry Observation recipe",
                    cr2res_obs_pol_description,
                    "Thomas Marquart, Yves Jung",
                    PACKAGE_BUGREPORT,
                    cr2res_get_license(),
                    cr2res_obs_pol_create,
                    cr2res_obs_pol_exec,
                    cr2res_obs_pol_destroy)) {    
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
static int cr2res_obs_pol_create(cpl_plugin * plugin)
{
    cpl_recipe          *   recipe ;
    cpl_parameter       *   p ;

    /* Check that the plugin is part of a valid recipe */
    if (cpl_plugin_get_type(plugin) == CPL_PLUGIN_TYPE_RECIPE)
        recipe = (cpl_recipe *)plugin;
    else
        return -1;

    /* Create the parameters list in the cpl_recipe object */
    recipe->parameters = cpl_parameterlist_new();

    /* Fill the parameters list */
    p = cpl_parameter_new_value("cr2res.cr2res_obs_pol.extract_oversample",
            CPL_TYPE_INT, "factor by which to oversample the extraction",
            "cr2res.cr2res_obs_pol", 2);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "extract_oversample");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_obs_pol.extract_swath_width",
            CPL_TYPE_INT, "The swath width", "cr2res.cr2res_obs_pol", 24);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "extract_swath_width");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_obs_pol.extract_height",
            CPL_TYPE_INT, "Extraction height",
            "cr2res.cr2res_obs_pol", -1);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "extract_height");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_obs_pol.extract_smooth",
            CPL_TYPE_DOUBLE,
            "Smoothing along the slit (1 for high S/N, 5 for low)",
            "cr2res.cr2res_obs_pol", 1.0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "extract_smooth");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_obs_pol.detector",
            CPL_TYPE_INT, "Only reduce the specified detector",
            "cr2res.cr2res_obs_pol", 0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "detector");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    return 0;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Execute the plugin instance given by the interface
  @param    plugin  the plugin
  @return   0 if everything is ok
 */
/*----------------------------------------------------------------------------*/
static int cr2res_obs_pol_exec(cpl_plugin * plugin)
{
    cpl_recipe  *recipe;

    /* Get the recipe out of the plugin */
    if (cpl_plugin_get_type(plugin) == CPL_PLUGIN_TYPE_RECIPE)
        recipe = (cpl_recipe *)plugin;
    else return -1;

    return cr2res_obs_pol(recipe->frames, recipe->parameters);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Destroy what has been created by the 'create' function
  @param    plugin  the plugin
  @return   0 if everything is ok
 */
/*----------------------------------------------------------------------------*/
static int cr2res_obs_pol_destroy(cpl_plugin * plugin)
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
static int cr2res_obs_pol(
        cpl_frameset            *   frameset,
        const cpl_parameterlist *   parlist)
{
    const cpl_parameter *   param ;
    int                     extract_oversample, extract_swath_width,
                            extract_height, reduce_det ;
    double                  extract_smooth ;
    cpl_frameset        *   rawframes ;
    cpl_frameset        *   raw_flat_frames ;
    const cpl_frame     *   trace_wave_frame ;
    const cpl_frame     *   detlin_frame ;
    const cpl_frame     *   master_dark_frame ;
    const cpl_frame     *   master_flat_frame ;
    const cpl_frame     *   bpm_frame ;
    cpl_table           *   pol_speca[CR2RES_NB_DETECTORS] ;
    cpl_table           *   pol_specb[CR2RES_NB_DETECTORS] ;
    cpl_propertylist    *   ext_plista[CR2RES_NB_DETECTORS] ;
    cpl_propertylist    *   ext_plistb[CR2RES_NB_DETECTORS] ;
    char                *   out_file;
    int                     i, det_nr; 

    /* Initialise */

    /* RETRIEVE INPUT PARAMETERS */
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_pol.extract_oversample");
    extract_oversample = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_pol.extract_swath_width");
    extract_swath_width = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_pol.extract_height");
    extract_height = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_pol.extract_smooth");
    extract_smooth = cpl_parameter_get_double(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_pol.detector");
    reduce_det = cpl_parameter_get_int(param);

    /* Identify the RAW and CALIB frames in the input frameset */
    if (cr2res_dfs_set_groups(frameset)) {
        cpl_msg_error(__func__, "Cannot identify RAW and CALIB frames") ;
        cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
        return -1 ;
    }
	
    /* Get Calibration frames */
    trace_wave_frame = cpl_frameset_find_const(frameset,
            CR2RES_FLAT_TRACE_WAVE_PROCATG) ;
    if (trace_wave_frame == NULL) {
        cpl_msg_error(__func__, "Could not find TRACE_WAVE frame") ;
        return -1 ;
    }
    detlin_frame = cpl_frameset_find_const(frameset,
            CR2RES_DETLIN_COEFFS_PROCATG);
    master_dark_frame = cpl_frameset_find_const(frameset,
            CR2RES_MASTER_DARK_PROCATG) ; 
    master_flat_frame = cpl_frameset_find_const(frameset,
            CR2RES_FLAT_MASTER_FLAT_PROCATG) ; 
    bpm_frame = cpl_frameset_find_const(frameset,
            CR2RES_FLAT_BPM_PROCATG) ;

    /* Get the RAW Frames */
    rawframes = cr2res_extract_frameset(frameset, CR2RES_OBS_POL_RAW) ;
    if (rawframes == NULL) {
        cpl_msg_error(__func__, "Could not find RAW frames") ;
        return -1 ;
    }
   
    /* Get the RAW flat frames */
    raw_flat_frames = cr2res_extract_frameset(frameset, CR2RES_FLAT_RAW) ;

    /* Loop on the detectors */
    for (det_nr=1 ; det_nr<=CR2RES_NB_DETECTORS ; det_nr++) {
        /* Initialise */
        pol_speca[det_nr-1] = NULL ;
        pol_specb[det_nr-1] = NULL ;
        ext_plista[det_nr-1] = NULL ;
        ext_plistb[det_nr-1] = NULL ;

        /* Compute only one detector */
        if (reduce_det != 0 && det_nr != reduce_det) continue ;
    
        cpl_msg_info(__func__, "Process Detector %d", det_nr) ;
        cpl_msg_indent_more() ;

        /* Call the reduction function */
        if (cr2res_obs_pol_reduce(rawframes, raw_flat_frames, trace_wave_frame, 
                    detlin_frame, master_dark_frame, master_flat_frame, 
                    bpm_frame, 0, extract_oversample, extract_swath_width, 
                    extract_height, extract_smooth, det_nr,
                    &(pol_speca[det_nr-1]),
                    &(pol_specb[det_nr-1]),
                    &(ext_plista[det_nr-1]),
                    &(ext_plistb[det_nr-1])) == -1) {
            cpl_msg_warning(__func__, "Failed to reduce detector %d", det_nr);
        }
        cpl_msg_indent_less() ;
    }

    /* Ѕave Products */
    out_file = cpl_sprintf("%s_pol_specA.fits", RECIPE_STRING) ;
    cr2res_io_save_POL_SPEC(out_file, frameset, rawframes, parlist,
            pol_speca, NULL, ext_plista, CR2RES_OBS_POL_SPECA_PROCATG, 
            RECIPE_STRING) ;
    cpl_free(out_file);

    out_file = cpl_sprintf("%s_pol_specB.fits", RECIPE_STRING) ;
    cr2res_io_save_POL_SPEC(out_file, frameset, rawframes, parlist,
            pol_specb, NULL, ext_plistb, CR2RES_OBS_POL_SPECB_PROCATG, 
            RECIPE_STRING) ;
    cpl_free(out_file);

    /* Free */
    cpl_frameset_delete(rawframes) ;
    if (raw_flat_frames != NULL) cpl_frameset_delete(raw_flat_frames) ;
    for (det_nr=1 ; det_nr<=CR2RES_NB_DETECTORS ; det_nr++) {
        if (pol_speca[det_nr-1] != NULL) 
            cpl_table_delete(pol_speca[det_nr-1]) ;
        if (pol_specb[det_nr-1] != NULL) 
            cpl_table_delete(pol_specb[det_nr-1]) ;
        if (ext_plista[det_nr-1] != NULL) 
            cpl_propertylist_delete(ext_plista[det_nr-1]) ;
        if (ext_plistb[det_nr-1] != NULL) 
            cpl_propertylist_delete(ext_plistb[det_nr-1]) ;
    }

    return (int)cpl_error_get_code();
}
 
/*----------------------------------------------------------------------------*/
/**
  @brief    Execute the polarimetry recipe on a specific detector
  @param rawframes              Raw science frames
  @param raw_flat_frames        Raw flat frames 
  @param trace_wave_frame       Trace Wave file
  @param detlin_frame           Associated detlin coefficients
  @param master_dark_frame      Associated master dark
  @param master_flat_frame      Associated master flat
  @param bpm_frame              Associated BPM
  @param calib_cosmics_corr     Flag to correct for cosmics
  @param extract_oversample     Extraction related
  @param extract_swath_width    Extraction related
  @param extract_height         Extraction related
  @param extract_smooth         Extraction related
  @param reduce_det             The detector to compute
  @param pol_speca              [out] polarimetry spectrum (A)
  @param pol_specb              [out] polarimetry spectrum (B)
  @param ext_plista             [out] the header for saving the products (A)
  @param ext_plistb             [out] the header for saving the products (B)
  @return  0 if ok, -1 otherwise
 */
/*----------------------------------------------------------------------------*/
static int cr2res_obs_pol_reduce(
        const cpl_frameset  *   rawframes,
        const cpl_frameset  *   raw_flat_frames,
        const cpl_frame     *   trace_wave_frame,
        const cpl_frame     *   detlin_frame,
        const cpl_frame     *   master_dark_frame,
        const cpl_frame     *   master_flat_frame,
        const cpl_frame     *   bpm_frame,
        int                     calib_cosmics_corr,
        int                     extract_oversample,
        int                     extract_swath_width,
        int                     extract_height,
        double                  extract_smooth,
        int                     reduce_det,
        cpl_table           **  pol_speca,
        cpl_table           **  pol_specb,
        cpl_propertylist    **  ext_plista,
        cpl_propertylist    **  ext_plistb)
{
    cpl_frameset        *   rawframes_a ;
    cpl_frameset        *   rawframes_b ;
    cr2res_nodding_pos  *   nod_positions ;
    cpl_table           *   pol_speca_loc ;
    cpl_table           *   pol_specb_loc ;
    cpl_propertylist    *   ext_plista_loc ;
    cpl_propertylist    *   ext_plistb_loc ;
    int                     i ;

    /* Check Inputs */
    if (pol_speca == NULL || pol_specb == NULL || ext_plista == NULL || 
            ext_plistb == NULL || rawframes == NULL || 
            trace_wave_frame == NULL) return -1 ;

    /* Get the Nodding positions */
    nod_positions = cr2res_nodding_read_positions(rawframes) ;

    /* START TMP Incomplete headers */
    cpl_free(nod_positions) ;
    nod_positions = cpl_malloc(12 * sizeof(cr2res_nodding_pos)) ;
    nod_positions[0] = CR2RES_NODDING_A ;
    nod_positions[1] = CR2RES_NODDING_A ;
    nod_positions[2] = CR2RES_NODDING_A ;
    nod_positions[3] = CR2RES_NODDING_A ;
    nod_positions[4] = CR2RES_NODDING_B ;
    nod_positions[5] = CR2RES_NODDING_B ;
    nod_positions[6] = CR2RES_NODDING_B ;
    nod_positions[7] = CR2RES_NODDING_B ;
    nod_positions[8] = CR2RES_NODDING_A ;
    nod_positions[9] = CR2RES_NODDING_A ;
    nod_positions[10] = CR2RES_NODDING_A ;
    nod_positions[11] = CR2RES_NODDING_A ;
    /* STOP TMP Incomplete headers */

    if (cpl_msg_get_level() == CPL_MSG_DEBUG) {
        for (i=0 ; i<cpl_frameset_get_size(rawframes) ; i++) {
            cpl_msg_debug(__func__, "Frame %s - Nodding %c",
                    cpl_frame_get_filename(
                        cpl_frameset_get_position_const(rawframes,i)),
                    cr2res_nodding_position_char(nod_positions[i])) ;
        }
    }

    /* Split the frames */
    if (cr2res_combine_nodding_split_frames(rawframes, nod_positions, 
                &rawframes_a, &rawframes_b)) {
        cpl_msg_error(__func__, "Failed to split the nodding positions") ;
        cpl_free(nod_positions) ;    
        return -1 ;
    }
    cpl_free(nod_positions) ;    

    /* Reduce A position */
    cpl_msg_error(__func__, "Compute Polarimetry for nodding A position") ;
    cpl_msg_indent_more() ;
    if (cr2res_obs_pol_reduce_one(rawframes_a, raw_flat_frames, 
                trace_wave_frame, detlin_frame, master_dark_frame, 
                master_flat_frame, bpm_frame, 0, extract_oversample, 
                extract_swath_width, extract_height, extract_smooth, reduce_det,
                &pol_speca_loc, &ext_plista_loc) == -1) {
        cpl_msg_error(__func__, "Failed to Reduce A nodding frames") ;
        if (rawframes_a != NULL) cpl_frameset_delete(rawframes_a);
        if (rawframes_b != NULL) cpl_frameset_delete(rawframes_b);
        cpl_msg_indent_less() ;
        return -1 ;
    }
    cpl_msg_indent_less() ;
    cpl_frameset_delete(rawframes_a);

    /* Reduce B position */
    cpl_msg_error(__func__, "Compute Polarimetry for nodding A position") ;
    cpl_msg_indent_more() ;
    if (cr2res_obs_pol_reduce_one(rawframes_b, raw_flat_frames, 
                trace_wave_frame, detlin_frame, master_dark_frame, 
                master_flat_frame, bpm_frame, 0, extract_oversample, 
                extract_swath_width, extract_height, extract_smooth, reduce_det,
                &pol_specb_loc, &ext_plistb_loc) == -1) {
        cpl_msg_error(__func__, "Failed to Reduce B nodding frames") ;
        if (rawframes_b != NULL) cpl_frameset_delete(rawframes_b);
        cpl_table_delete(pol_speca_loc) ;
        cpl_propertylist_delete(ext_plista_loc) ;
        cpl_msg_indent_less() ;
        return -1 ;
    }
    cpl_msg_indent_less() ;
    cpl_frameset_delete(rawframes_b);

    *pol_speca = pol_speca_loc ;
    *pol_specb = pol_specb_loc ;
    *ext_plista = ext_plista_loc ;
    *ext_plistb = ext_plistb_loc ;
    return 0 ;
}
 
/*----------------------------------------------------------------------------*/
/**
  @brief Execute the polarimetry computation for 1 nodding position
  @param rawframes              Raw science frames for 1 position
  @param raw_flat_frames        Raw flat frames 
  @param trace_wave_frame       Trace Wave file
  @param detlin_frame           Associated detlin coefficients
  @param master_dark_frame      Associated master dark
  @param master_flat_frame      Associated master flat
  @param bpm_frame              Associated BPM
  @param calib_cosmics_corr     Flag to correct for cosmics
  @param extract_oversample     Extraction related
  @param extract_swath_width    Extraction related
  @param extract_height         Extraction related
  @param extract_smooth         Extraction related
  @param reduce_det             The detector to compute
  @param pol_spec               [out] polarimetry spectrum
  @param ext_plist              [out] the header for saving the products
  @return  0 if ok, -1 otherwise
 */
/*----------------------------------------------------------------------------*/
static int cr2res_obs_pol_reduce_one(
        const cpl_frameset  *   rawframes,
        const cpl_frameset  *   raw_flat_frames,
        const cpl_frame     *   trace_wave_frame,
        const cpl_frame     *   detlin_frame,
        const cpl_frame     *   master_dark_frame,
        const cpl_frame     *   master_flat_frame,
        const cpl_frame     *   bpm_frame,
        int                     calib_cosmics_corr,
        int                     extract_oversample,
        int                     extract_swath_width,
        int                     extract_height,
        double                  extract_smooth,
        int                     reduce_det,
        cpl_table           **  pol_spec,
        cpl_propertylist    **  ext_plist)
{
    cpl_vector          *   dits ;
    hdrl_imagelist      *   in ;
    hdrl_imagelist      *   in_calib ;
    cpl_table           *   trace_wave ;
    cpl_table           *   trace_wave_corrected ;
    cpl_table           *   extracted[2*CR2RES_POLARIMETRY_GROUP_SIZE];
    cpl_table           **  pol_spec_one_group ;
    cpl_table           **  extract_1d ;
    cpl_vector          **  intens ;
    cpl_vector          **  wl ;
    cpl_vector          **  errors ;
    cpl_bivector        **  demod_stokes ;
    cpl_bivector        **  demod_null ;
    cpl_bivector        **  demod_intens ;
    int                 *   orders ;
    cpl_table           *   pol_spec_merged ;
    cpl_size                nframes, nspec_group ;
    int                     ngroups, i, j, k, o, norders, frame_idx ;

    /* Check Inputs */
    if (pol_spec == NULL || ext_plist == NULL || rawframes == NULL || 
            trace_wave_frame == NULL) return -1 ;

    /* Check number of frames */
    nframes = cpl_frameset_get_size(rawframes) ;
    if (nframes == 0 || nframes % CR2RES_POLARIMETRY_GROUP_SIZE) {
        cpl_msg_error(__func__, 
    "Input number of frames is %"CPL_SIZE_FORMAT" and should be multiple of %d",
            nframes, CR2RES_POLARIMETRY_GROUP_SIZE) ;
        return -1 ;
    }

    /* Load the DITs if necessary */
    if (master_dark_frame != NULL)  dits = cr2res_read_dits(rawframes) ;
    else                            dits = NULL ;
    if (cpl_msg_get_level() == CPL_MSG_DEBUG && dits != NULL) 
        cpl_vector_dump(dits, stdout) ;

    /* Load image list */
    if ((in = cr2res_io_load_image_list_from_set(rawframes, 
                    reduce_det)) == NULL) {
        cpl_msg_error(__func__, "Cannot load images") ;
        if (dits != NULL) cpl_vector_delete(dits) ;
        return -1 ;
    }
    if (hdrl_imagelist_get_size(in) != cpl_frameset_get_size(rawframes)) {
        cpl_msg_error(__func__, "Inconsistent number of loaded images") ;
        if (dits != NULL) cpl_vector_delete(dits) ;
        hdrl_imagelist_delete(in) ;
        return -1 ;
    }

    /* Calibrate the images */
    if ((in_calib = cr2res_calib_imagelist(in, reduce_det, 0, master_flat_frame,
            master_dark_frame, bpm_frame, detlin_frame, dits)) == NULL) {
        cpl_msg_error(__func__, "Failed to apply the calibrations") ;
        if (dits != NULL) cpl_vector_delete(dits) ;
        hdrl_imagelist_delete(in) ;
        return -1 ;
    }
    hdrl_imagelist_delete(in) ;
    if (dits != NULL) cpl_vector_delete(dits) ;

    /* Load the trace wave */
    cpl_msg_info(__func__, "Load the TRACE WAVE") ;
    if ((trace_wave = cr2res_io_load_TRACE_WAVE(cpl_frame_get_filename(
                        trace_wave_frame), reduce_det)) == NULL) {
        cpl_msg_error(__func__, "Failed to Load the traces file") ;
        hdrl_imagelist_delete(in_calib) ;
        return -1 ;
    }

    /* Correct trace_wave with some provided raw flats */
    if (raw_flat_frames != NULL) {
        cpl_msg_info(__func__, "Try to correct the reproducibility error") ;
        cpl_msg_indent_more() ;
        trace_wave_corrected = cr2res_trace_adjust(trace_wave, raw_flat_frames, 
                reduce_det) ;
        if (trace_wave_corrected != NULL) {
            cpl_table_delete(trace_wave) ;
            trace_wave = trace_wave_corrected ;
            trace_wave_corrected = NULL ;
        }
        cpl_msg_indent_less() ;
    }

    /* Compute the number of groups */
    ngroups = nframes/CR2RES_POLARIMETRY_GROUP_SIZE ;

    /* Allocate pol_spec_group containers */
    pol_spec_one_group = cpl_malloc(ngroups * sizeof(cpl_table*)) ;

    /* Loop on the groups */
    for (i=0 ; i<ngroups ; i++) {
        cpl_msg_info(__func__, "Process %d-group number %d/%d", 
                CR2RES_POLARIMETRY_GROUP_SIZE, i+1, ngroups) ;

        /* Initialise */
        pol_spec_one_group[i] = NULL ;
        nspec_group = 2*CR2RES_POLARIMETRY_GROUP_SIZE ;

        /* Container for extracted tables: 1u, 1d, 2u, 2d, 3u, 3d, 4u, 4d */
        extract_1d = cpl_malloc(nspec_group * sizeof(cpl_table*));

        /* Loop on the frames */
        for (j=0 ; j<CR2RES_POLARIMETRY_GROUP_SIZE ; j++) {
            frame_idx = i * CR2RES_POLARIMETRY_GROUP_SIZE + j ;

            /* Extract up and down */
            /* TODO */
            extract_1d[2*j] = cpl_table_new(1);
            extract_1d[2*j+1] = cpl_table_new(1);
        }

        /* How many orders */
        /* TODO  get orders/norders that are in all 8 exracted tables*/
        norders = 5 ;
        orders = cpl_malloc(norders * sizeof(int)) ;
        orders[0] = 3;
        orders[1] = 4;
        orders[2] = 5;
        orders[3] = 6;
        orders[4] = 7;

        /* Allocate data containerѕ */
        demod_stokes = cpl_malloc(norders * sizeof(cpl_bivector*)) ;
        demod_null = cpl_malloc(norders * sizeof(cpl_bivector*)) ;
        demod_intens = cpl_malloc(norders * sizeof(cpl_bivector*)) ;

        /* Loop on the orders */
        for (o=0 ; o<norders ; o++) {
            
            /* Get the inputs for the demod functions calls */
            intens = cpl_malloc(nspec_group * sizeof(cpl_vector*));
            wl = cpl_malloc(nspec_group * sizeof(cpl_vector*));
            errors = cpl_malloc(nspec_group * sizeof(cpl_vector*));
            for (k=0 ; k<nspec_group ; k++) {
               
                /* TODO */

            }

            
            /* Call demod functions */
            demod_stokes[o]=cr2res_pol_demod_stokes(intens,wl,errors,norders) ;
            demod_null[o]=cr2res_pol_demod_null(intens,wl,errors,norders) ;
            demod_intens[o]=cr2res_pol_demod_intens(intens,wl,errors,norders) ;

            /* Free */
            for (k=0 ; k<nspec_group ; k++) {
                cpl_vector_delete(intens[k]) ;
                cpl_vector_delete(wl[k]) ;
                cpl_vector_delete(errors[k]) ;
            }
            cpl_free(intens) ;
            cpl_free(wl) ;
            cpl_free(errors) ;
        }

        /* Create the pol_spec table */
        pol_spec_one_group[i] = cr2res_pol_POL_SPEC_create(orders,
                demod_stokes, demod_null, demod_intens, norders) ;

        /* Deallocate */
        for (o=0 ; o<norders ; o++) {
            cpl_bivector_delete(demod_stokes[o]) ;
            cpl_bivector_delete(demod_null[o]) ;
            cpl_bivector_delete(demod_intens[o]) ;
        }
        cpl_free(demod_stokes) ;
        cpl_free(demod_null) ;
        cpl_free(demod_intens) ;
    }
    cpl_table_delete(trace_wave) ;
    hdrl_imagelist_delete(in_calib) ;

    /* Merge the groups together */


    /* TODO QCs */

    /* Return */

    *pol_spec = pol_spec_merged ;
    /* TODO  : *ext_plist */
    return 0 ;
}




    /* Compute the slit fractions for A and B positions extraction */   
    /*
	The assumption is made here that :
		- The slit center is exactly in the middle of A and B poѕitions
        - The nodthrow iѕ the distance in arcseconds between A and B
        - The slit size is 10 arcseconds
        - The A position is above the B position
    */
    /*
    slit_length = 10 ;
    if ((plist = cpl_propertylist_load(cpl_frame_get_filename(
                        cpl_frameset_get_position_const(rawframes, 0)),
                    0)) == NULL) {
        cpl_msg_error(__func__, "Cannot read the NODTHROW in the input files") ;
        hdrl_image_delete(collapsed_a) ;
        hdrl_image_delete(collapsed_b) ;
        cpl_table_delete(trace_wave) ;
        return -1 ;
    }
    nod_throw = cr2res_pfits_get_nodthrow(plist) ;
    cpl_propertylist_delete(plist) ;
    if (nod_throw < slit_length / 2.0) {
        extr_width_frac = nod_throw/slit_length ;  
        slit_frac_a_bot = 0.5 ;
        slit_frac_a_mid = slit_frac_a_bot + extr_width_frac/2.0 ;
        slit_frac_a_top = slit_frac_a_bot + extr_width_frac ;
        slit_frac_b_top = 0.5 ;
        slit_frac_b_mid = slit_frac_b_top - extr_width_frac/2.0 ;
        slit_frac_b_bot = slit_frac_b_top - extr_width_frac ;
    } else if (nod_throw <= slit_length) {
        extr_width_frac = (slit_length - nod_throw)/slit_length ;  
        slit_frac_a_top = 1.0 ;
        slit_frac_a_mid = slit_frac_a_top - extr_width_frac/2.0 ;
        slit_frac_a_bot = slit_frac_a_top - extr_width_frac ;
        slit_frac_b_bot = 0.0 ;
        slit_frac_b_mid = slit_frac_b_bot + extr_width_frac/2.0 ;
        slit_frac_b_top = slit_frac_b_bot + extr_width_frac ;
    } else {
        cpl_msg_error(__func__, "NODTHROW > slit length (%g>%g)- abort", 
                nod_throw, slit_length) ;
        hdrl_image_delete(collapsed_a) ;
        hdrl_image_delete(collapsed_b) ;
        cpl_table_delete(trace_wave) ;
        return -1 ;
    }
    cpl_msg_info(__func__, "Nodding A extraction: Slit fraction %g - %g",
            slit_frac_a_bot, slit_frac_a_top) ;
    cpl_msg_info(__func__, "Nodding B extraction: Slit fraction %g - %g",
            slit_frac_b_bot, slit_frac_b_top) ;

    slit_frac_a = cpl_array_new(3, CPL_TYPE_DOUBLE) ;
    cpl_array_set(slit_frac_a, 0, slit_frac_a_bot) ;
    cpl_array_set(slit_frac_a, 1, slit_frac_a_mid) ;
    cpl_array_set(slit_frac_a, 2, slit_frac_a_top) ;

    slit_frac_b = cpl_array_new(3, CPL_TYPE_DOUBLE) ;
    cpl_array_set(slit_frac_b, 0, slit_frac_b_bot) ;
    cpl_array_set(slit_frac_b, 1, slit_frac_b_mid) ;
    cpl_array_set(slit_frac_b, 2, slit_frac_b_top) ;
        */

    /* Recompute the traces for the new slit fractions */
    /*
    if ((trace_wave_a = cr2res_trace_new_slit_fraction(trace_wave,
            slit_frac_a)) == NULL) {
        cpl_msg_error(__func__, 
                "Failed to compute the traces for extraction of A") ;
        hdrl_image_delete(collapsed_a) ;
        hdrl_image_delete(collapsed_b) ;
        cpl_table_delete(trace_wave) ;
        cpl_array_delete(slit_frac_a) ;
        cpl_array_delete(slit_frac_b) ;
        return -1 ;
    }
    cpl_array_delete(slit_frac_a) ;
    if ((trace_wave_b = cr2res_trace_new_slit_fraction(trace_wave,
            slit_frac_b)) == NULL) {
        cpl_msg_error(__func__, 
                "Failed to compute the traces for extraction of B") ;
        hdrl_image_delete(collapsed_a) ;
        hdrl_image_delete(collapsed_b) ;
        cpl_table_delete(trace_wave) ;
        cpl_table_delete(trace_wave_a) ;
        cpl_array_delete(slit_frac_b) ;
        return -1 ;
    }
    cpl_array_delete(slit_frac_b) ;
    cpl_table_delete(trace_wave) ;
        */

    /* Execute the extraction */
    /*
    cpl_msg_info(__func__, "Spectra Extraction") ;
    if (cr2res_extract_traces(collapsed_a, trace_wave_a, -1, -1,
                CR2RES_EXTR_OPT_CURV, extract_height, extract_swath_width, 
                extract_oversample, extract_smooth,
                &extracted_a, &slit_func_a, &model_master_a) == -1) {
        cpl_msg_error(__func__, "Failed to extract A");
        hdrl_image_delete(collapsed_a) ;
        hdrl_image_delete(collapsed_b) ;
        cpl_table_delete(trace_wave_a) ;
        cpl_table_delete(trace_wave_b) ;
        return -1 ;
    }
    */
/* TODO : Save trace_wave_a and b as products */
    /*
    cpl_table_delete(trace_wave_a) ;
    if (cr2res_extract_traces(collapsed_b, trace_wave_b, -1, -1,
                CR2RES_EXTR_OPT_CURV, extract_height, extract_swath_width, 
                extract_oversample, extract_smooth,
                &extracted_b, &slit_func_b, &model_master_b) == -1) {
        cpl_msg_error(__func__, "Failed to extract B");
        cpl_table_delete(extracted_a) ;
        cpl_table_delete(slit_func_a) ;
        hdrl_image_delete(model_master_a) ;
        hdrl_image_delete(collapsed_a) ;
        hdrl_image_delete(collapsed_b) ;
        cpl_table_delete(trace_wave_b) ;
        return -1 ;
    }
    cpl_table_delete(trace_wave_b) ;
    */

/*----------------------------------------------------------------------------*/
/**
  @brief  Run basic checks for the rawframes consistency
  @param    rawframes   The input rawframes
  @return   1 if valid, 0 if not, -1 in error case
 */
/*----------------------------------------------------------------------------*/
static int cr2res_obs_pol_check_inputs_validity(
        const cpl_frameset  *   rawframes)
{
    cpl_propertylist        *   plist ;
    cr2res_nodding_pos      *   nod_positions ;
    cpl_size                    nframes, i ;
    double                      nodthrow, nodthrow_cur ;
    int                         nb_a, nb_b ;

    /* Check Inputs */
    if (rawframes == NULL) return -1 ;

    /* number of frames must be modulo 4 */
    nframes = cpl_frameset_get_size(rawframes) ;
    if (nframes % 4) {
        cpl_msg_error(__func__, "Require a multiple of 4 raw frames") ;
        return 0 ;
    }

    /* TODO : use cr2res_combine_nodding_split_frames() */

    /* Clarify what the decker requirements are */

    /* Require Decker frames */
    /* TODO */


    /* Need same number of A and B positions */
    nb_a = nb_b = 0 ;
    nod_positions = cr2res_nodding_read_positions(rawframes) ;
    if (nod_positions == NULL) return -1 ;
    for (i=0 ; i<nframes ; i++) {
        if (nod_positions[i] == CR2RES_NODDING_A) nb_a++ ;
        if (nod_positions[i] == CR2RES_NODDING_B) nb_b++ ;
    }
    cpl_free(nod_positions) ;    

    if (nb_a == 0 || nb_b == 0 || nb_a != nb_b) {
        cpl_msg_error(__func__, "Require as many A and B positions") ;
        return 0 ;
    }

    /* Need the same nod throw in all frames */
    if ((plist = cpl_propertylist_load(cpl_frame_get_filename(
                        cpl_frameset_get_position_const(rawframes, 0)),
                    0)) == NULL) {
        return -1;
    } 
    nodthrow = cr2res_pfits_get_nodthrow(plist);
    cpl_propertylist_delete(plist) ;
    for (i=1 ; i<nframes ; i++) {
        if ((plist = cpl_propertylist_load(cpl_frame_get_filename(
                            cpl_frameset_get_position_const(rawframes, i)),
                        0)) == NULL) {
            return -1;
        } 
        nodthrow_cur = cr2res_pfits_get_nodthrow(plist);
        cpl_propertylist_delete(plist) ;

        if (fabs(nodthrow_cur-nodthrow) > 1e-3) {
            cpl_msg_error(__func__, 
                    "Require constant NOD THROW in the raw frames") ;
            return 0 ;
        }
    }
    return 1 ;
}


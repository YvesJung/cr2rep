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
#include "cr2res_calib.h"
#include "cr2res_pfits.h"
#include "cr2res_dfs.h"
#include "cr2res_bpm.h"
#include "cr2res_trace.h"
#include "cr2res_extract.h"
#include "cr2res_io.h"

/*-----------------------------------------------------------------------------
                                Define
 -----------------------------------------------------------------------------*/

#define RECIPE_STRING "cr2res_obs_1d"

/*-----------------------------------------------------------------------------
                             Plugin registration
 -----------------------------------------------------------------------------*/

int cpl_plugin_get_info(cpl_pluginlist * list);

/*-----------------------------------------------------------------------------
                            Private function prototypes
 -----------------------------------------------------------------------------*/

static int cr2res_obs_1d_reduce(
        const cpl_frameset  *   rawframes,
        const cpl_frame     *   detlin_frame,
        const cpl_frame     *   master_dark_frame,
        const cpl_frame     *   bpm_frame,
        const cpl_frame     *   trace_wave_frame,
        int                     extract_oversample,
        int                     extract_swath_width,
        int                     extract_height,
        double                  extract_smooth,
        int                     extract_sum_only,
        int                     reduce_det,
        int                     reduce_order,
        int                     reduce_trace,
        cpl_table           **  extract_1d_noda,
        cpl_table           **  extract_1d_nodb,
        hdrl_image          **  combined_noda,
        hdrl_image          **  combined_nodb,
        cpl_propertylist    **  plist) ;
static int cr2res_obs_1d_create(cpl_plugin *);
static int cr2res_obs_1d_exec(cpl_plugin *);
static int cr2res_obs_1d_destroy(cpl_plugin *);
static int cr2res_obs_1d(cpl_frameset *, const cpl_parameterlist *);

/*-----------------------------------------------------------------------------
                            Static variables
 -----------------------------------------------------------------------------*/

static char cr2res_obs_1d_description[] =
"CRIRES+ 1d Observation recipe\n"
"The files listed in the Set Of Frames (sof-file) must be tagged:\n"
"raw-file.fits " CR2RES_OBS1D_RAW "\n"
"detlin.fits " CR2RES_DETLIN_COEFFS_PROTYPE "\n"
"master_dark.fits " CR2RES_MASTER_DARK_PROTYPE "\n"
"bpm.fits " CR2RES_BPM_PROTYPE "\n"
"trace_wave.fits " CR2RES_TRACE_WAVE_PROTYPE "\n"
" The recipe produces the following products:\n"
"cr2res_obs_1d_extractA.fits " CR2RES_OBS1D_EXTRACTA_PROCATG "\n"
"cr2res_obs_1d_extractB.fits " CR2RES_OBS1D_EXTRACTB_PROCATG "\n"
"cr2res_obs_1d_combinedA.fits " CR2RES_OBS1D_COMBINEDA_PROCATG "\n"
"cr2res_obs_1d_combinedB.fits " CR2RES_OBS1D_COMBINEDB_PROCATG "\n"
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
                    "cr2res_obs_1d",
                    "1D Observation recipe",
                    cr2res_obs_1d_description,
                    "Thomas Marquart, Yves Jung",
                    PACKAGE_BUGREPORT,
                    cr2res_get_license(),
                    cr2res_obs_1d_create,
                    cr2res_obs_1d_exec,
                    cr2res_obs_1d_destroy)) {    
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
static int cr2res_obs_1d_create(cpl_plugin * plugin)
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
    p = cpl_parameter_new_value("cr2res.cr2res_obs_1d.extract_oversample",
            CPL_TYPE_INT, "factor by which to oversample the extraction",
            "cr2res.cr2res_obs_1d", 2);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "extract_oversample");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_obs_1d.extract_swath_width",
            CPL_TYPE_INT, "The swath width", "cr2res.cr2res_obs_1d", 24);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "extract_swath_width");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_obs_1d.extract_height",
            CPL_TYPE_INT, "Extraction height",
            "cr2res.cr2res_obs_1d", -1);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "extract_height");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_obs_1d.extract_smooth",
            CPL_TYPE_DOUBLE,
            "Smoothing along the slit (1 for high S/N, 5 for low)",
            "cr2res.cr2res_obs_1d", 1.0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "extract_smooth");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_obs_1d.detector",
            CPL_TYPE_INT, "Only reduce the specified detector",
            "cr2res.cr2res_obs_1d", 0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "detector");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_obs_1d.order",
            CPL_TYPE_INT, "Only reduce the specified order",
            "cr2res.cr2res_obs_1d", -1);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "order");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_obs_1d.trace_nb",
            CPL_TYPE_INT, "Only reduce the specified trace number",
            "cr2res.cr2res_obs_1d", -1);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "trace_nb");
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
static int cr2res_obs_1d_exec(cpl_plugin * plugin)
{
    cpl_recipe  *recipe;

    /* Get the recipe out of the plugin */
    if (cpl_plugin_get_type(plugin) == CPL_PLUGIN_TYPE_RECIPE)
        recipe = (cpl_recipe *)plugin;
    else return -1;

    return cr2res_obs_1d(recipe->frames, recipe->parameters);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Destroy what has been created by the 'create' function
  @param    plugin  the plugin
  @return   0 if everything is ok
 */
/*----------------------------------------------------------------------------*/
static int cr2res_obs_1d_destroy(cpl_plugin * plugin)
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
static int cr2res_obs_1d(
        cpl_frameset            *   frameset,
        const cpl_parameterlist *   parlist)
{
    const cpl_parameter *   param ;
    int                     extract_oversample, extract_swath_width,
                            extract_height, reduce_det, reduce_order, 
                            reduce_trace ;
    double                  extract_smooth ;
    cpl_frameset        *   rawframes ;
    const cpl_frame     *   detlin_frame ;
    const cpl_frame     *   master_dark_frame ;
    const cpl_frame     *   bpm_frame ;
    hdrl_image          *   combineda[CR2RES_NB_DETECTORS] ;
    hdrl_image          *   combinedb[CR2RES_NB_DETECTORS] ;
    cpl_propertylist    *   ext_plist[CR2RES_NB_DETECTORS] ;
    cpl_table           *   extracta[CR2RES_NB_DETECTORS] ;
    cpl_table           *   extractb[CR2RES_NB_DETECTORS] ;
    char                *   out_file;
    int                     i, det_nr; 

    /* Initialise */

    /* RETRIEVE INPUT PARAMETERS */
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_1d.extract_oversample");
    extract_oversample = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_1d.extract_swath_width");
    extract_swath_width = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_1d.extract_height");
    extract_height = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_1d.extract_smooth");
    extract_smooth = cpl_parameter_get_double(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_1d.detector");
    reduce_det = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_1d.order");
    reduce_order = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_obs_1d.trace_nb");
    reduce_trace = cpl_parameter_get_int(param);

    /* Identify the RAW and CALIB frames in the input frameset */
    if (cr2res_dfs_set_groups(frameset)) {
        cpl_msg_error(__func__, "Cannot identify RAW and CALIB frames") ;
        cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
        return -1 ;
    }
	
    /* Get Calibration frames */
    detlin_frame = cpl_frameset_find_const(frameset,
            CR2RES_DETLIN_COEFFS_PROCATG);
    master_dark_frame = cpl_frameset_find_const(frameset,
            CR2RES_MASTER_DARK_PROCATG) ; 
    bpm_frame = cpl_frameset_find_const(frameset,
            CR2RES_FLAT_BPM_PROCATG) ;
    trace_wave_frame = cpl_frameset_find_const(frameset,
            CR2RES_FLAT_TRACE_WAVE_PROCATG) ;

    /* Initialise */
    for (det_nr=1 ; det_nr<=CR2RES_NB_DETECTORS ; det_nr++) {
        ext_plist[det_nr-1] = NULL ;
    }

    /* Get the Frames for the current decker position */
    rawframes = cr2res_extract_decker_frameset(frameset,
            CR2RES_FLAT_RAW, decker_values[i]) ;
    if (rawframes == NULL) continue ;
    cpl_msg_info(__func__, "Reduce %s Frames", decker_desc[i]) ;
    cpl_msg_indent_more() ;

    /* Loop on the detectors */
    for (det_nr=1 ; det_nr<=CR2RES_NB_DETECTORS ; det_nr++) {
        /* Initialise */
        master_flat[det_nr-1] = NULL ;
        slit_func[det_nr-1] = NULL ;
        extract_1d[det_nr-1] = NULL ;
        slit_model[det_nr-1] = NULL ;
        bpm[det_nr-1] = NULL ;

        /* Compute only one detector */
        if (reduce_det != 0 && det_nr != reduce_det) continue ;
    
        cpl_msg_info(__func__, "Process Detector %d", det_nr) ;
        cpl_msg_indent_more() ;

        /* Call the reduction function */
        if (cr2res_obs_1d_reduce(rawframes, detlin_frame, 
                    master_dark_frame, bpm_frame, calib_cosmics_corr,
                    bpm_low, bpm_high, bpm_lines_ratio,
                    trace_degree, trace_min_cluster, trace_smooth, 
                    trace_opening, extract_oversample, 
                    extract_swath_width, extract_height, extract_smooth, 0,
                    det_nr, reduce_order, reduce_trace,
                    &(master_flat[det_nr-1]),
                    &(trace_wave[i][det_nr-1]),
                    &(slit_func[det_nr-1]),
                    &(extract_1d[det_nr-1]),
                    &(slit_model[det_nr-1]),
                    &(bpm[det_nr-1]),
                    &(ext_plist[i][det_nr-1])) == -1) {
            cpl_msg_warning(__func__, 
                    "Failed to reduce detector %d of %s Frames", 
                    det_nr, decker_desc[i]);
        }
        cpl_msg_indent_less() ;
    }
    cpl_frameset_delete(rawframes) ;

    /* Ѕave Products */

    /* SLIT_MODEL */
    out_file = cpl_sprintf("%s_%s_slit_model.fits", RECIPE_STRING,
            decker_desc[i]) ;
    cr2res_io_save_SLIT_MODEL(out_file, frameset, parlist, slit_model, 
            NULL, ext_plist[i], CR2RES_FLAT_SLIT_MODEL_PROCATG, 
            RECIPE_STRING) ;
    cpl_free(out_file);

    /* BLAZE */
    out_file = cpl_sprintf("%s_%s_blaze.fits", RECIPE_STRING,
            decker_desc[i]) ;
    cr2res_io_save_EXTRACT_1D(out_file, frameset, parlist, extract_1d, NULL,
            ext_plist[i], CR2RES_FLAT_EXTRACT_1D_PROCATG, RECIPE_STRING) ;
    cpl_free(out_file);

    /* MASTER_FLAT */
    out_file = cpl_sprintf("%s_%s_master_flat.fits", RECIPE_STRING,
            decker_desc[i]) ;
    cr2res_io_save_MASTER_FLAT(out_file, frameset, parlist, master_flat, 
            NULL, ext_plist[i], CR2RES_FLAT_MASTER_FLAT_PROCATG, 
            RECIPE_STRING) ;
    cpl_free(out_file);

    /* TRACE_WAVE */
    out_file = cpl_sprintf("%s_%s_trace_wave.fits", RECIPE_STRING,
            decker_desc[i]) ;
    cr2res_io_save_TRACE_WAVE(out_file, frameset, parlist, trace_wave[i], 
            NULL, ext_plist[i], CR2RES_FLAT_TRACE_WAVE_PROCATG, 
            RECIPE_STRING) ;
    cpl_free(out_file);

    /* SLIT_FUNC */
    out_file = cpl_sprintf("%s_%s_slit_func.fits", RECIPE_STRING,
            decker_desc[i]) ;
    cr2res_io_save_SLIT_FUNC(out_file, frameset, parlist, slit_func, NULL, 
            ext_plist[i], CR2RES_FLAT_SLIT_FUNC_PROCATG, RECIPE_STRING) ;
    cpl_free(out_file);

    /* BPM */
    out_file = cpl_sprintf("%s_%s_master_bpm.fits", RECIPE_STRING,
            decker_desc[i]) ;
    cr2res_io_save_BPM(out_file, frameset, parlist, bpm, NULL, ext_plist[i],
            CR2RES_FLAT_BPM_PROCATG, RECIPE_STRING) ;
    cpl_free(out_file);

    /* Free */
    for (det_nr=1 ; det_nr<=CR2RES_NB_DETECTORS ; det_nr++) {
        if (master_flat[det_nr-1] != NULL)
            hdrl_image_delete(master_flat[det_nr-1]) ;
        if (slit_func[det_nr-1] != NULL) 
            cpl_table_delete(slit_func[det_nr-1]) ;
        if (extract_1d[det_nr-1] != NULL)
            cpl_table_delete(extract_1d[det_nr-1]) ;
        if (slit_model[det_nr-1] != NULL) 
            hdrl_image_delete(slit_model[det_nr-1]) ;
        if (bpm[det_nr-1] != NULL) 
            cpl_image_delete(bpm[det_nr-1]) ;
    }
    cpl_msg_indent_less() ;


    /* Save TRACE_WAVE_MERGED */
    out_file = cpl_sprintf("%s_trace_wave_merged.fits", RECIPE_STRING) ;
    cr2res_io_save_TRACE_WAVE(out_file, frameset, parlist, trace_wave_merged,
            NULL, ext_plist[0], CR2RES_FLAT_TRACE_WAVE_MERGED_PROCATG, 
            RECIPE_STRING) ;
    cpl_free(out_file);

    /* Free extensions */
    for (i=0 ; i<CR2RES_NB_DECKER_POSITIONS ; i++) 
        for (det_nr=1 ; det_nr<=CR2RES_NB_DETECTORS ; det_nr++) 
            if (ext_plist[i][det_nr-1] != NULL) 
                cpl_propertylist_delete(ext_plist[i][det_nr-1]) ;

    return (int)cpl_error_get_code();
}

/*----------------------------------------------------------------------------*/
/**
  @brief  
  @param 
  @return  
 */
/*----------------------------------------------------------------------------*/
static int cr2res_obs_1d_reduce(
        const cpl_frameset  *   rawframes,
        const cpl_frame     *   detlin_frame,
        const cpl_frame     *   master_dark_frame,
        const cpl_frame     *   bpm_frame,
        int                     calib_cosmics_corr,
        double                  bpm_low,
        double                  bpm_high,
        double                  bpm_linemax,
        int                     trace_degree,
        int                     trace_min_cluster,
        double                  trace_smooth,
        int                     trace_opening,
        int                     extract_oversample,
        int                     extract_swath_width,
        int                     extract_height,
        double                  extract_smooth,
        int                     extract_sum_only,
        int                     reduce_det,
        int                     reduce_order,
        int                     reduce_trace,
        hdrl_image          **  master_flat,
        cpl_table           **  trace_wave,
        cpl_table           **  slit_func,
        cpl_table           **  extract_1d,
        hdrl_image          **  slit_model,
        cpl_image           **  bpm,
        cpl_propertylist    **  ext_plist)
{
    const char          *   first_file ;
    cpl_imagelist       *   imlist ;
    hdrl_image          *   collapsed ;
    cpl_image           *   collapsed_ima ;
    cpl_propertylist    *   plist ;
    hdrl_image          *   master_flat_loc ;
    cpl_image           *   bpm_im ;
    cpl_mask            *   bpm_flat ;
    cpl_table           *   traces ;
    cpl_bivector          **  spectrum ;
    cpl_vector          **  slit_func_vec ;
    hdrl_image          *   model_master;
    cpl_table           *   slit_func_tab ;
    cpl_table           *   extract_tab ;
    hdrl_image          *   model_tmp ;
    double                  dit ;
    int                     i, ext_nr, nb_traces, order, trace_id ;
    
    /* Check Inputs */
    if (rawframes == NULL) return -1 ;


    /* Get the Extension number */
    first_file = cpl_frame_get_filename(
            cpl_frameset_get_position_const(rawframes, 0)) ;
    ext_nr = cr2res_io_get_ext_idx(first_file, reduce_det, 1) ;

    /* Get the DIT for the Dark correction */
    plist = cpl_propertylist_load(first_file, 0) ;
    dit = cr2res_pfits_get_dit(plist) ;
    cpl_propertylist_delete(plist);

    /* Load the extension header for saving */
    plist = cpl_propertylist_load(first_file, ext_nr) ;
    if (plist == NULL) return -1 ;

    /* Load the image list */
    imlist = cpl_imagelist_load_frameset(rawframes, CPL_TYPE_FLOAT, 1, ext_nr) ;
    if (imlist == NULL) {
        cpl_msg_error(__func__, "Failed to Load the images") ;
        cpl_propertylist_delete(plist);
        return -1 ;
    }

    /* Calibrate the Data */
    cpl_msg_info(__func__, "Calibrate the input images") ;
    cpl_msg_indent_more() ;
    if (cr2res_calib_chip_list(imlist, reduce_det, calib_cosmics_corr, NULL,
                master_dark_frame, bpm_frame, detlin_frame, dit) != 0) {
        cpl_msg_error(__func__, "Failed to Calibrate the Data") ;
        cpl_propertylist_delete(plist);
        cpl_imagelist_delete(imlist) ;
        cpl_msg_indent_less() ;
        return -1 ;
    }
    cpl_msg_indent_less() ;

    /* Collapse */
    cpl_msg_info(__func__, "Collapse the input images") ;
    cpl_msg_indent_more() ;
    if ((collapsed_ima = cpl_imagelist_collapse_create(imlist)) == NULL) {
        cpl_msg_error(__func__, "Failed to Calibrate and collapse") ;
        cpl_propertylist_delete(plist);
        cpl_imagelist_delete(imlist) ;
        cpl_msg_indent_less() ;
        return -1 ;
    }
    cpl_imagelist_delete(imlist) ;

    /* Create the HDRL collapsed */
    collapsed = hdrl_image_create(collapsed_ima, NULL) ;
    cpl_image_delete(collapsed_ima) ;
    cpl_msg_indent_less() ;

    /* Compute traces */
    cpl_msg_info(__func__, "Compute the traces") ;
    cpl_msg_indent_more() ;
    if ((traces = cr2res_trace(hdrl_image_get_image(collapsed), 
                    trace_smooth, trace_opening, trace_degree, 
                    trace_min_cluster)) == NULL) {
        cpl_msg_error(__func__, "Failed compute the traces") ;
        cpl_propertylist_delete(plist);
        hdrl_image_delete(collapsed) ;
        cpl_msg_indent_less() ;
        return -1 ;
    }
    cpl_msg_indent_less() ;

    /* Add The remaining Columns to the trace table */
    cr2res_trace_add_extra_columns(traces, first_file, reduce_det) ;

    /* Extract */
    nb_traces = cpl_table_get_nrow(traces) ;
	spectrum = cpl_malloc(nb_traces * sizeof(cpl_bivector *)) ;
	slit_func_vec = cpl_malloc(nb_traces * sizeof(cpl_vector *)) ;
	model_master = hdrl_image_duplicate(collapsed) ;
	hdrl_image_mul_scalar(model_master, (hdrl_value){0.0, 0.0}) ;

	/* Loop over the traces and extract them */
    cpl_msg_info(__func__, "Extract the traces") ;
    cpl_msg_indent_more() ;
	for (i=0 ; i<nb_traces ; i++) {
		/* Initialise */
		slit_func_vec[i] = NULL ;
		spectrum[i] = NULL ;
		model_tmp = NULL ;

		/* Get Order and trace id */
		order = cpl_table_get(traces, CR2RES_COL_ORDER, i, NULL) ;
		trace_id = cpl_table_get(traces, CR2RES_COL_TRACENB, i, NULL) ;

		/* Check if this order needs to be skipped */
		if (reduce_order > -1 && order != reduce_order) continue ;

		/* Check if this trace needs to be skipped */
		if (reduce_trace > -1 && trace_id != reduce_trace) continue ;

		cpl_msg_info(__func__, "Process Order %d/Trace %d", order, trace_id) ;
		cpl_msg_indent_more() ;
 
		/* Call the Extraction */
		if (extract_sum_only) {
			/* Call the SUM ONLY extraction */
			if (cr2res_extract_sum_vert(collapsed, 
                        traces, order, trace_id, extract_height, 
                        &(slit_func_vec[i]), &(spectrum[i]), &model_tmp) != 0) {
				cpl_msg_error(__func__, "Cannot (sum-)extract the trace") ;
				slit_func_vec[i] = NULL ;
				spectrum[i] = NULL ;
				model_tmp = NULL ;
				cpl_error_reset() ;
				cpl_msg_indent_less() ;
				continue ;
			}
		} else {
			/* Call the SLIT DECOMPOSITION */
			if (cr2res_extract_slitdec_vert(
                        collapsed, 
                        traces, order, trace_id, extract_height, 
                        extract_swath_width, extract_oversample, 
                        extract_smooth, &(slit_func_vec[i]), &(spectrum[i]), 
                        &model_tmp) != 0) {
				cpl_msg_error(__func__, "Cannot (slitdec-) extract the trace") ;
				slit_func_vec[i] = NULL ;
				spectrum[i] = NULL ;
				model_tmp = NULL ;
				cpl_error_reset() ;
				cpl_msg_indent_less() ;
				continue ;
			}
		}

		/* Update the model global image */
		if (model_tmp != NULL) {
			hdrl_image_add_image(model_master, model_tmp) ;
			hdrl_image_delete(model_tmp) ;
		}
		cpl_msg_indent_less() ;
	}
    cpl_msg_indent_less() ;

	/* Create the slit_func_tab for the current detector */
	slit_func_tab = cr2res_extract_SLITFUNC_create(slit_func_vec, traces) ;

	/* Create the extracted_tab for the current detector */
	extract_tab = cr2res_extract_EXTRACT1D_create(spectrum, traces) ;

	/* Deallocate Vectors */
	for (i=0 ; i<nb_traces ; i++) {
		if (slit_func_vec[i] != NULL) cpl_vector_delete(slit_func_vec[i]) ;
		if (spectrum[i] != NULL) cpl_bivector_delete(spectrum[i]) ;
	}
	cpl_free(spectrum) ;
	cpl_free(slit_func_vec) ;

    /* Compute the Master flat */
    cpl_msg_info(__func__, "Compute the master flat") ;
    cpl_msg_indent_more() ;
    if ((master_flat_loc = cr2res_master_flat(collapsed, 
                    model_master, bpm_low, bpm_high, bpm_linemax,
                    &bpm_flat)) == NULL) {
        cpl_msg_error(__func__, "Failed compute the Master Flat") ;
        cpl_table_delete(slit_func_tab) ;
        cpl_table_delete(extract_tab) ;
        hdrl_image_delete(model_master) ;
        cpl_propertylist_delete(plist);
        hdrl_image_delete(collapsed) ;
		cpl_table_delete(traces) ;
        cpl_msg_indent_less() ;
        return -1 ;
    }
    cpl_msg_indent_less() ;
    hdrl_image_delete(collapsed) ;

    /* Create BPM image */
    bpm_im = NULL ;
    if (bpm_frame != NULL) {
        if ((bpm_im = cr2res_io_load_BPM(
                        cpl_frame_get_filename(bpm_frame),
                        reduce_det)) == NULL) {
            cpl_msg_warning(__func__, "Failed to Load the Master BPM") ;
        }
    }
    if (bpm_im == NULL) {
        bpm_im = cpl_image_new(cpl_mask_get_size_x(bpm_flat),
                cpl_mask_get_size_y(bpm_flat), CPL_TYPE_INT) ;
    }
    
    /* Add the flat BPM to the BPM image */
    if (cr2res_bpm_add_mask(bpm_im, bpm_flat, CR2RES_BPM_FLAT)) {
        cpl_msg_error(__func__, "Failed to add the mask to the BPM") ;
        cpl_table_delete(slit_func_tab) ;
        cpl_table_delete(extract_tab) ;
        hdrl_image_delete(model_master) ;
        hdrl_image_delete(master_flat_loc) ;
        cpl_propertylist_delete(plist);
        cpl_image_delete(bpm_im) ;
        cpl_mask_delete(bpm_flat) ;
		cpl_table_delete(traces) ;
        cpl_msg_indent_less() ;
        return -1 ;
    }
    cpl_mask_delete(bpm_flat) ;

    /* Return the results */
    *trace_wave = traces ;
    *slit_func = slit_func_tab ;
    *extract_1d = extract_tab ;
    *slit_model = model_master ;
    *master_flat = master_flat_loc ;
    *ext_plist = plist ;
    *bpm = bpm_im ;
    return 0 ;
}

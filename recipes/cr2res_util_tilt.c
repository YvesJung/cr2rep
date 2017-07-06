
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

#include <string.h>

#include <cpl.h>
#include "hdrl.h"

#include "cr2res_tilt.h"
#include "cr2res_utils.h"
#include "cr2res_pfits.h"
#include "cr2res_dfs.h"
#include "cr2res_io.h"

/*-----------------------------------------------------------------------------
                                Define
 -----------------------------------------------------------------------------*/

#define RECIPE_STRING "cr2res_util_tilt"

/*-----------------------------------------------------------------------------
                             Plugin registration
 -----------------------------------------------------------------------------*/

int cpl_plugin_get_info(cpl_pluginlist * list);

/*-----------------------------------------------------------------------------
                            Private function prototypes
 -----------------------------------------------------------------------------*/

static int cr2res_util_tilt_create(cpl_plugin *);
static int cr2res_util_tilt_exec(cpl_plugin *);
static int cr2res_util_tilt_destroy(cpl_plugin *);
static int cr2res_util_tilt(cpl_frameset *, const cpl_parameterlist *);

/*-----------------------------------------------------------------------------
                            Static variables
 -----------------------------------------------------------------------------*/

static char cr2res_util_tilt_description[] =
"The utility expects 1 file as input:\n"
"   * trace_wave.fits " CR2RES_COMMAND_LINE "\n"
"The slit tilt is derived from each order with more than 1 trace.\n"
"The recipe produces the following products:\n"
"   * SLIT_TILT\n"
"\n";

/*-----------------------------------------------------------------------------
                                Function code
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @defgroup cr2res_util_tilt    Slit Tilt
 */
/*----------------------------------------------------------------------------*/

/**@{*/

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
                    "cr2res_util_tilt",
                    "Slit Tilt",
                    cr2res_util_tilt_description,
                    "Thomas Marquart, Yves Jung",
                    PACKAGE_BUGREPORT,
                    cr2res_get_license(),
                    cr2res_util_tilt_create,
                    cr2res_util_tilt_exec,
                    cr2res_util_tilt_destroy)) {
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
static int cr2res_util_tilt_create(cpl_plugin * plugin)
{
    cpl_recipe    * recipe;
    cpl_parameter * p;

    /* Check that the plugin is part of a valid recipe */
    if (cpl_plugin_get_type(plugin) == CPL_PLUGIN_TYPE_RECIPE)
        recipe = (cpl_recipe *)plugin;
    else
        return -1;

    /* Create the parameters list in the cpl_recipe object */
    recipe->parameters = cpl_parameterlist_new();

    /* Fill the parameters list */
    p = cpl_parameter_new_value("cr2res.cr2res_util_tilt.detector",
            CPL_TYPE_INT, "Only reduce the specified detector",
            "cr2res.cr2res_util_tilt", 0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "detector");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_util_tilt.order",
            CPL_TYPE_INT, "Only reduce the specified order",
            "cr2res.cr2res_util_tilt", -1);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "order");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_util_tilt.display",
            CPL_TYPE_BOOL, "Flag for display",
            "cr2res.cr2res_util_tilt", FALSE);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "display");
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
static int cr2res_util_tilt_exec(cpl_plugin * plugin)
{
    cpl_recipe  *recipe;

    /* Get the recipe out of the plugin */
    if (cpl_plugin_get_type(plugin) == CPL_PLUGIN_TYPE_RECIPE)
        recipe = (cpl_recipe *)plugin;
    else return -1;

    return cr2res_util_tilt(recipe->frames, recipe->parameters);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Destroy what has been created by the 'create' function
  @param    plugin  the plugin
  @return   0 if everything is ok
 */
/*----------------------------------------------------------------------------*/
static int cr2res_util_tilt_destroy(cpl_plugin * plugin)
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
static int cr2res_util_tilt(
        cpl_frameset            *   frameset,
        const cpl_parameterlist *   parlist)
{
    const cpl_parameter *   param;
    int                     reduce_det, reduce_order, display ;
    cpl_frame           *   fr ;
    const char          *   trace_wave_file ;
    cpl_table           *   trace_wave_table ;
    cpl_table           *   out_tilt[CR2RES_NB_DETECTORS] ;
    int                 *   orders ;
    int                     det_nr, nb_orders ;
    cpl_polynomial      **  order_tilts ;
    char                *   out_file;
    int                     i, j ;

    /* RETRIEVE INPUT PARAMETERS */
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_util_tilt.detector");
    reduce_det = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_util_tilt.order");
    reduce_order = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_util_tilt.display");
    display = cpl_parameter_get_bool(param) ;
 
    /* Check Parameters */
    /* TODO */

    /* Identify the RAW and CALIB frames in the input frameset */
    if (cr2res_dfs_set_groups(frameset) != CPL_ERROR_NONE) {
        cpl_msg_error(__func__, "Cannot identify RAW and CALIB frames") ;
        cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
        return -1 ;
    }

    /* Get Inputs */
    fr = cpl_frameset_get_position(frameset, 0);
    trace_wave_file = cpl_frame_get_filename(fr) ;
    if (trace_wave_file==NULL) {
        cpl_msg_error(__func__, "The utility needs at least 1 file as input");
        cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
        return -1 ;
    }

    /* Loop over the detectors */
    for (det_nr=1 ; det_nr<=CR2RES_NB_DETECTORS ; det_nr++) {

        /* Initialise */
        out_tilt[det_nr-1] = NULL ;

        /* Compute only one detector */
        if (reduce_det != 0 && det_nr != reduce_det) continue ;

        cpl_msg_info(__func__, "Process detector number %d", det_nr) ;
        cpl_msg_indent_more() ;

        /* Load the TRACE_WAVE table of this detector */
        cpl_msg_info(__func__, "Load the TRACE_WAVE table") ;
        if ((trace_wave_table = cr2res_io_load_TRACE_WAVE(trace_wave_file,
                        det_nr)) == NULL) {
            cpl_msg_error(__func__,"Failed to load table - skip detector");
            cpl_error_reset() ;
            cpl_msg_indent_less() ;
            continue ;
        }

        /* Get the list of orders  */
        orders = cr2res_get_trace_table_orders(trace_wave_table, &nb_orders) ;

        /* Loop over the Orders */
        for (i=0 ; i<nb_orders ; i++) {
            /* Check if this order needs to be skipped */
            if (reduce_order > -1 && orders[i] != reduce_order) {
                continue ;
            }

            cpl_msg_info(__func__, "Process Order %d", orders[i]) ;
            cpl_msg_indent_more() ;

            /* Call the Tilt Computation */
            if ((order_tilts = cr2res_tilt(trace_wave_table, orders[i],
                            display))==NULL) {
                cpl_msg_error(__func__, "Cannot Compute Tilt") ;
                cpl_error_reset() ;
                cpl_msg_indent_less() ;
                continue ;
            }

            /* Store the Solution in the table */
            /* TODO */


            for (j=0 ; j<CR2RES_DETECTOR_SIZE ; j++) 
                if (order_tilts[j] != NULL)
                    cpl_polynomial_delete(order_tilts[j]) ;
            cpl_free(order_tilts) ; 

            cpl_msg_indent_less() ;
        }
        cpl_free(orders) ;
        cpl_table_delete(trace_wave_table) ;

        cpl_msg_indent_less() ;
    }

    /* Save the new SLIT_TILT table */
    out_file=cpl_sprintf("%s_tilt.fits", cr2res_get_root_name(trace_wave_file));
    cr2res_io_save_TILT_POLY(out_file, frameset, parlist, out_tilt, NULL, 
            RECIPE_STRING) ;
    cpl_free(out_file);

    /* Free and return */
    for (i=0 ; i<CR2RES_NB_DETECTORS ; i++) 
        if (out_tilt[i] != NULL) cpl_table_delete(out_tilt[i]) ;
    return (int)cpl_error_get_code();
}

/**@}*/
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*-----------------------------------------------------------------------------
                                Includes
 -----------------------------------------------------------------------------*/

#include <string.h>
#include <math.h>

#include <cpl.h>
#include "hdrl.h"
#include "irplib_wlxcorr.h"

#include "cr2res_utils.h"
#include "cr2res_pfits.h"
#include "cr2res_dfs.h"
#include "cr2res_io.h"

/*-----------------------------------------------------------------------------
                                Define
 -----------------------------------------------------------------------------*/

#define RECIPE_STRING "cr2res_util_plot"

/*-----------------------------------------------------------------------------
                             Plugin registration
 -----------------------------------------------------------------------------*/

int cpl_plugin_get_info(cpl_pluginlist * list);

/*-----------------------------------------------------------------------------
                            Functions prototypes
 -----------------------------------------------------------------------------*/

static int cr2res_util_plot_spec_1d(cpl_table *, cpl_table *, int, int, int);
static int cr2res_util_plot_spec_1d_one(cpl_table *, cpl_table *, 
        const char *, int, const char *, const char *, const char *) ;
static int cr2res_util_plot_create(cpl_plugin *);
static int cr2res_util_plot_exec(cpl_plugin *);
static int cr2res_util_plot_destroy(cpl_plugin *);
static int cr2res_util_plot(cpl_frameset *, const cpl_parameterlist *);

/*-----------------------------------------------------------------------------
                            Static variables
 -----------------------------------------------------------------------------*/

static char cr2res_util_plot_description[] = "Plot the CR2RES tables.\n" 
"This recipe accepts possibly 2 parameter:\n"
"First parameter:       the table to plot.\n"
"                       (PRO TYPE = "CR2RES_PROTYPE_CATALOG") or\n"
"                       (PRO TYPE = "CR2RES_EXTRACT_1D_PROTYPE") or\n"
"                       (PRO TYPE = "CR2RES_PROTYPE_XCORR") or\n"
"Second parameter is optional and must be of the same type and same\n"
"                 table length as the first one. If provided, the two\n"
"                 signals are overplotted. In this case, --adjust can \n"
"                 be used to adjust the second plot average level to \n"
"                 the first one.\n" ;

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
                    RECIPE_STRING,
                    "Plotting utility",
                    cr2res_util_plot_description,
                    "Thomas Marquart, Yves Jung",
                    PACKAGE_BUGREPORT,
                    cr2res_get_license(),
                    cr2res_util_plot_create,
                    cr2res_util_plot_exec,
                    cr2res_util_plot_destroy)) {
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
static int cr2res_util_plot_create(cpl_plugin * plugin)
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
    p = cpl_parameter_new_value("cr2res.cr2res_util_plot.xmin",
            CPL_TYPE_DOUBLE, "Minimum x value to plot",
            "cr2res.cr2res_util_plot", -1.0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "xmin");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_util_plot.xmax",
            CPL_TYPE_DOUBLE, "Maximum x value to plot",
            "cr2res.cr2res_util_plot", -1.0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "xmax");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_util_plot.detector",
            CPL_TYPE_INT, "Only reduce the specified detector",
            "cr2res.cr2res_util_plot", 0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "detector");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_util_plot.order",
            CPL_TYPE_INT, "Only reduce the specified order",
            "cr2res.cr2res_util_plot", -1);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "order");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_util_plot.trace_nb",
            CPL_TYPE_INT, "Only reduce the specified trace number",
            "cr2res.cr2res_util_plot", -1);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "trace_nb");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_util_plot.adjust_level",
            CPL_TYPE_BOOL, "Flag to adjust the level with 2 plots",
            "cr2res.cr2res_util_plot", TRUE);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "adjust_level");
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
static int cr2res_util_plot_exec(cpl_plugin * plugin)
{
    cpl_recipe  *recipe;

    /* Get the recipe out of the plugin */
    if (cpl_plugin_get_type(plugin) == CPL_PLUGIN_TYPE_RECIPE)
        recipe = (cpl_recipe *)plugin;
    else return -1;

    return cr2res_util_plot(recipe->frames, recipe->parameters);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Destroy what has been created by the 'create' function
  @param    plugin  the plugin
  @return   0 if everything is ok
 */
/*----------------------------------------------------------------------------*/
static int cr2res_util_plot_destroy(cpl_plugin * plugin)
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
  @param    parlist     the parameters list
  @return   0 if everything is ok
 */
/*----------------------------------------------------------------------------*/
static int cr2res_util_plot(
        cpl_frameset            *   frameset,
        const cpl_parameterlist *   parlist)
{
    const cpl_parameter *   param;
    int                     xmin, xmax, reduce_det, reduce_trace,
                            reduce_order, adjust ;
    const char          *   fname ;
    const char          *   fname_opt ;
    const char          *   title ;
    const char          *   protype ;
    cpl_propertylist    *   plist ;
    cpl_table           *   tab ;
    cpl_table           *   sel_tab ;
    cpl_table           *   tab_opt ;

    /* RETRIEVE INPUT PARAMETERS */
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_util_plot.xmin");
    xmin = cpl_parameter_get_double(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_util_plot.xmax");
    xmax = cpl_parameter_get_double(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_util_plot.detector");
    reduce_det = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_util_plot.order");
    reduce_order = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_util_plot.trace_nb");
    reduce_trace = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_util_plot.adjust_level");
    adjust = cpl_parameter_get_bool(param);

    /* Initialise */
    
    /* Retrieve raw frames */
    fname = cpl_frame_get_filename(cpl_frameset_get_position(frameset, 0)) ;

    /* Read the PRO.TYPE */
    plist = cpl_propertylist_load(fname, 0) ;
    protype = cr2res_pfits_get_protype(plist) ;

    /* CR2RES_PROTYPE_CATALOG */
    if (!strcmp(protype, CR2RES_PROTYPE_CATALOG)) {
        title = "t 'Emission lines' w lines" ;
        tab = cpl_table_load(fname, 1, 0) ;

        /* Sub selection of the catalog */
        if (xmin > 0.0 && xmax >.0) {
            cpl_table_and_selected_double(tab, CR2RES_COL_WAVELENGTH,
                    CPL_GREATER_THAN, xmin) ;
            cpl_table_and_selected_double(tab, CR2RES_COL_WAVELENGTH,
                    CPL_LESS_THAN, xmax) ;
            sel_tab = cpl_table_extract_selected(tab) ;
            cpl_table_delete(tab) ;
            tab = sel_tab ;
        }

        /* Plot */
        cpl_plot_column(
                "set grid;set xlabel 'Wavelength (nm)';set ylabel 'Emission';",
                title, "", tab, CR2RES_COL_WAVELENGTH, CR2RES_COL_EMISSION) ;
        cpl_table_delete(tab) ;
    }
 
    /* CR2RES_EXTRACT_1D_PROTYPE */
    if (!strcmp(protype, CR2RES_EXTRACT_1D_PROTYPE)) {
        /* Load the table */
        tab = cr2res_load_table(fname, reduce_det, (int)xmin, (int)xmax) ;
        cr2res_util_plot_spec_1d(tab, tab_opt, adjust, reduce_order, 
                reduce_trace) ;
        cpl_table_delete(tab) ;
    }

   /* CR2RES_PROTYPE_XCORR */
    if (!strcmp(protype, CR2RES_PROTYPE_XCORR)) {
        /* Load the table */
        tab = cr2res_load_table(fname, reduce_det, (int)xmin, (int)xmax) ;
        irplib_wlxcorr_plot_spc_table(tab, "", 1, 5) ;
        cpl_table_delete(tab) ;
    }

    /* Delete */
    cpl_propertylist_delete(plist) ;

    /* Return */
    if (cpl_error_get_code()) 
        return -1 ;
    else 
        return 0 ;
}

static int cr2res_util_plot_spec_1d(
        cpl_table   *       tab,
        cpl_table   *       tab_opt,
        int                 adjust,
        int                 order,
        int                 trace) 
{
    char    *   wl_col ;
    char    *   spec_col ;
    char    *   err_col ;

    /* Protect empty chips in windowing mode */
    if (cpl_table_get_nrow(tab) == 0) {
        return 0 ;
    }

    /* Get column names */
    spec_col = cr2res_dfs_SPEC_colname(order, trace) ;
    err_col = cr2res_dfs_SPEC_ERR_colname(order, trace) ;
    wl_col = cr2res_dfs_WAVELENGTH_colname(order, trace) ;

    /* SPECTRUM */
    cr2res_util_plot_spec_1d_one(tab, tab_opt, wl_col, adjust, spec_col,
"set grid;set xlabel 'Wavelength (nm)';set ylabel 'Intensity (ADU/sec)';",
            "t 'Extracted Spectrum' w lines") ;

    /* ERROR */
    cr2res_util_plot_spec_1d_one(tab, tab_opt, wl_col, adjust, err_col,
"set grid;set xlabel 'Wavelength (nm)';set ylabel 'Intensity (ADU/sec)';",
            "t 'Error Spectrum' w lines") ;

    cpl_free(spec_col);
    cpl_free(err_col);
    cpl_free(wl_col);
    return 0 ;
}
 
static int cr2res_util_plot_spec_1d_one(
        cpl_table   *       tab,
        cpl_table   *       tab_opt,
        const char  *       wave_col,
        int                 adjust_level,
        const char  *       y_col,
        const char  *       options,
        const char  *       title)
{
    double                  mean1, mean2 ;
    int                     nrows ;
    cpl_vector          **  vectors ;

    /* Check inputs */
    if (tab == NULL) return -1 ;
    nrows = cpl_table_get_nrow(tab) ;
    if (tab_opt != NULL) {
        if (cpl_table_get_nrow(tab_opt) != nrows) {
            cpl_msg_error(__func__, 
                    "The two tables must have the same number of rows") ;
            return -1 ;
        }
    }
    if (tab_opt != NULL) {
        vectors = cpl_malloc(3*sizeof(cpl_vector*)) ;
        vectors[0]=cpl_vector_wrap(nrows,cpl_table_get_data_double(tab, 
                    wave_col));
        vectors[1] = cpl_vector_wrap(nrows, 
                cpl_table_get_data_double(tab, y_col)) ;
        vectors[2] = cpl_vector_wrap(nrows,
                cpl_table_get_data_double(tab_opt, y_col));
        if (adjust_level) {
            mean1 = cpl_vector_get_mean(vectors[1]) ;
            mean2 = cpl_vector_get_mean(vectors[2]) ;
            cpl_vector_multiply_scalar(vectors[2], fabs(mean1/mean2)) ;
        }
        cpl_plot_vectors(options, title, "", (const cpl_vector **)vectors, 3);
        cpl_vector_unwrap(vectors[0]) ;
        cpl_vector_unwrap(vectors[1]) ;
        cpl_vector_unwrap(vectors[2]) ;
        cpl_free(vectors) ;
    } else {
        cpl_plot_column(options, title, "", tab, wave_col, y_col) ;
    }
    return 0 ;
}


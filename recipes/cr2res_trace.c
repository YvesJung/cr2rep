/*
 * This file is part of the CR2RE Pipeline
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

#include "cr2re_utils.h"
#include "cr2re_pfits.h"
#include "cr2re_cluster.h"
#include "cr2re_dfs.h"

/*-----------------------------------------------------------------------------
                            Private function prototypes
 -----------------------------------------------------------------------------*/

static int cr2res_trace_create(cpl_plugin *);
static int cr2res_trace_exec(cpl_plugin *);
static int cr2res_trace_destroy(cpl_plugin *);
static int cr2res_trace(cpl_frameset *, const cpl_parameterlist *);

/*-----------------------------------------------------------------------------
                            Static variables
 -----------------------------------------------------------------------------*/

static char cr2res_trace_description[] =
"This example text is used to describe the recipe.\n"
"The description should include the required FITS-files and\n"
"their associated tags, e.g.\n"
"raw-file.fits " CR2RE_TRACE_RAW "\n"
"\n"
"Additionally, it should describe functionality of the expected output."
"\n";

/*----------------------------------------------------------------------------*/
/**
 * @defgroup cr2re_trace   TODO
 */
/*----------------------------------------------------------------------------*/

/**@{*/

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

    cpl_plugin_init(plugin,
            CPL_PLUGIN_API,
            CR2RE_BINARY_VERSION,
            CPL_PLUGIN_TYPE_RECIPE,
            "cr2res_trace",
            "Tracing programm",
            cr2res_trace_description,
            "Thomas Marquart",
            PACKAGE_BUGREPORT,
            cr2re_get_license(),
            cr2res_trace_create,
            cr2res_trace_exec,
            cr2res_trace_destroy) ;

    cpl_pluginlist_append(list, plugin);

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
static int cr2res_trace_create(cpl_plugin * plugin)
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
    p = cpl_parameter_new_value("cr2res.cr2res_trace.poly_order",
            CPL_TYPE_INT,
            "polynomial order for the fit to the orders",
            "cr2res.cr2res_trace", 4);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "polyorder");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_trace.min_cluster",
            CPL_TYPE_INT,
            "size (number of pixels) of the smallest allowed cluster",
            "cr2res.cr2res_trace", 40);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "mincluster");
    cpl_parameter_disable(p, CPL_PARAMETER_MODE_ENV);
    cpl_parameterlist_append(recipe->parameters, p);

    p = cpl_parameter_new_value("cr2res.cr2res_trace.smooth",
            CPL_TYPE_DOUBLE,
            "Length of the smoothing kernel, relative to inter-order separation",
            "cr2res.cr2res_trace", 1.0);
    cpl_parameter_set_alias(p, CPL_PARAMETER_MODE_CLI, "smooth");
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
static int cr2res_trace_exec(cpl_plugin * plugin)
{
    cpl_recipe  *recipe;

    /* Get the recipe out of the plugin */
    if (cpl_plugin_get_type(plugin) == CPL_PLUGIN_TYPE_RECIPE)
        recipe = (cpl_recipe *)plugin;
    else return -1;

    return cr2res_trace(recipe->frames, recipe->parameters);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Destroy what has been created by the 'create' function
  @param    plugin  the plugin
  @return   0 if everything is ok
 */
/*----------------------------------------------------------------------------*/
static int cr2res_trace_destroy(cpl_plugin * plugin)
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
static int cr2res_trace(
        cpl_frameset            * frameset,
        const cpl_parameterlist * parlist)
{
    const cpl_parameter *   param;
    int                     polyorder;
    int                     mincluster;
    double                  smoothfactor;
    const cpl_frame     *   rawframe;
    double                  qc_param;
    cpl_propertylist    *   plist;
    cpl_propertylist    *   applist;
    cpl_image           *   image;
    cpl_imagelist       *   imlist;
    cpl_mask            *   mask;
    cpl_size                npix;
    cpl_matrix          *   kernel;
    int                 *   xs;
    int                 *   ys;
    int                 *   clusters;
    int                     i, j, nx, ny, nclusters, count ;
    /* This needs to come from a static calibration, each band */
    int                     ordersep=180;

    /* Check entries */
    if (parlist == NULL || frameset == NULL) {
        cpl_msg_error(__func__, "Null Inputs") ;
        cpl_error_set(__func__, CPL_ERROR_NULL_INPUT) ;
        return -1 ;
    }

    /* Get Parameters */
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_trace.min_cluster");
    mincluster = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_trace.poly_order");
    polyorder = cpl_parameter_get_int(param);
    param = cpl_parameterlist_find_const(parlist,
            "cr2res.cr2res_trace.smooth");
    smoothfactor = cpl_parameter_get_double(param);

    /* Check Parameters */
    /* Identify the RAW and CALIB frames in the input frameset */
    if (cr2re_dfs_set_groups(frameset) != CPL_ERROR_NONE) {
        cpl_msg_error(__func__, "Cannot identify RAW and CALIB frames") ;
        cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
        return -1 ;
    }

    /* Load the images */
    imlist = cpl_imagelist_load_frameset(frameset, CPL_TYPE_DOUBLE, 0, 0);
    if (imlist== NULL) {
        cpl_msg_error(__func__, "Cannot Load images") ;
        cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
        return -1 ;
    }

    rawframe = cpl_frameset_get_position(frameset,0);
    plist = cpl_propertylist_load(cpl_frame_get_filename(rawframe), 0);
    if (plist == NULL) {
        cpl_imagelist_delete(imlist) ;
        cpl_msg_error(__func__, "Could not read the FITS header") ;
        cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
        return -1 ;
    }

    qc_param = cr2re_pfits_get_dit(plist);

    /* NOW PERFORMING THE DATA REDUCTION */
    image = cpl_imagelist_get(imlist,0);
    if (image == NULL) {
        cpl_imagelist_delete(imlist) ;
        cpl_propertylist_delete(plist) ;
        cpl_msg_error(__func__, "Could not get image out of imagelist") ;
        cpl_error_set(__func__, CPL_ERROR_ILLEGAL_INPUT) ;
        return -1 ;
    }

    applist = cpl_propertylist_duplicate(plist);

    /* Add the product category  */
    cpl_propertylist_append_string(applist, CPL_DFS_PRO_CATG,
            CR2RE_TRACE_PROCATG);

    /* Detect the orders */
    mask = cr2re_cluster_detect(image, ordersep, smoothfactor) ;

    cpl_mask_save(mask, "mask.fits", plist, CPL_IO_CREATE);

    /* Convert the Mask in inputs needed by cluster() */
    npix = cpl_mask_count(mask);
    nx = cpl_mask_get_size_x(mask);
    ny = cpl_mask_get_size_y(mask);
    cpl_msg_debug(__func__, "mask: %d %d, %d", nx, ny, npix);

    xs=(int *)cpl_malloc(npix*sizeof(int));
    ys=(int *)cpl_malloc(npix*sizeof(int));
    clusters=(int *)cpl_malloc(npix*sizeof(int));
    count = 0 ;
    for (i=1 ; i<=nx ; i++) {
        for (j=1 ; j<=ny ; j++) {
            if (cpl_mask_get(mask,i,j) == CPL_BINARY_1) {
                xs[count] = i ;
                ys[count] = j ;
                count++;
            }
        }
    }
    cpl_mask_delete(mask);

    /* Call cluster() */
    nclusters = cluster(xs, ys, npix, nx, ny, mincluster, clusters) ;

    /* Convert the results back into a CPL image */
    for (i=0 ; i<npix ; i++) cpl_image_set(image, xs[i], ys[i], clusters[i]);
    cpl_free(xs);
    cpl_free(ys);
    cpl_free(clusters);

    if (cpl_dfs_save_image(frameset, plist, parlist, frameset, NULL, image,
                CPL_BPP_IEEE_FLOAT, "cr2res_trace", applist, NULL, 
                PACKAGE "/" PACKAGE_VERSION, "cr2res_trace.fits")) {
        /* Propagate the error */
        (void)cpl_error_set_where(cpl_func);
    }

    cpl_imagelist_delete(imlist) ;
    cpl_propertylist_delete(plist) ;
    cpl_propertylist_delete(applist) ;

    return (int)cpl_error_get_code();
}

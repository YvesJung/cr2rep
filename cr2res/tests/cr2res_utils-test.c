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

#include <stdlib.h>
#include <string.h>
#include <cpl.h>
#include <cr2res_utils.h>
#include <cr2res_dfs.h>

/*-----------------------------------------------------------------------------
                                Functions prototypes
 -----------------------------------------------------------------------------*/

static void test_cr2res_vector_get_rest(void);        //check
static void test_cr2res_vector_get_int(void);         //check
static void test_cr2res_image_cut_rectify(void);      //check
static void test_cr2res_image_insert_rect(void);      //check
static void test_cr2res_polynomial_eval_vector(void); //check

static void test_cr2res_threshold_spec(void);
static void test_cr2res_get_base_name(void);
static void test_cr2res_get_root_name(void);
static void test_cr2res_extract_filename(void);
static void test_cr2res_extract_frameset(void);
static void test_cr2res_wlestimate_compute(void);
static void test_cr2res_get_trace_wave_poly(void);
static void test_cr2res_get_trace_table_index(void);
static void test_cr2res_convert_order_to_idx(void);
static void test_cr2res_convert_idx_to_order(void);
static void test_cr2res_convert_array_to_poly(void);
static void test_cr2res_convert_poly_to_array(void);
static void test_cr2res_get_trace_table_orders(void);
static void test_cr2res_detector_shotnoise_model(void);

static void test_cr2res_get_license(void); //check

/*----------------------------------------------------------------------------*/
/**
 * @defgroup cr2res_utils-test    Unit test of cr2res_utils
 *
 */
/*----------------------------------------------------------------------------*/

/**@{*/

/*----------------------------------------------------------------------------*/
/**
  @brief
  @param
  @return
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_vector_get_int(void)
{
    int i;
    double d;
    int n = 10;
    cpl_vector *in = cpl_vector_new(n);
    int *res;

    for (i = 0; i < n; i++)
    {
        d = (double)i;
        cpl_vector_set(in, i, d + (d / (n + 1)));
    }

    cpl_test(res = cr2res_vector_get_int(in));

    for (i = 0; i < n; i++)
    {
        cpl_test_eq(i, res[i]);
    }

    cpl_vector_delete(in);
    cpl_free(res);

    return;
}

/*----------------------------------------------------------------------------*/
/**
  @brief
  @param
  @return
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_vector_get_rest(void)
{
    int i;
    double d;
    int n = 1000;
    cpl_vector *in = cpl_vector_new(n);
    cpl_vector *out = cpl_vector_new(n);
    double *res;

    for (i = 0; i < n; i++)
    {
        d = (double)i;
        cpl_vector_set(in, i, d + (d / (n + 1)));
        cpl_vector_set(out, i, (d / (n + 1)));
    }

    cpl_test(res = cr2res_vector_get_rest(in));
    cpl_vector_delete(in);
    in = cpl_vector_wrap(n, res);
    cpl_test_vector_abs(in, out, DBL_EPSILON * n);

    cpl_vector_delete(in);
    cpl_vector_delete(out);

    return;
}

/*----------------------------------------------------------------------------*/
/**
  @brief
  @param
  @return
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_image_cut_rectify(void)
{
    cpl_image *res;
    cpl_image *cmp;

    int imdata[] = {1, 2, 3, 2, 1,
                    1, 2, 9, 2, 9,
                    1, 9, 3, 9, 1,
                    9, 2, 3, 2, 1};
    cpl_image *img = cpl_image_wrap_int(5, 4, imdata);
    cpl_image_flip(img, 0); // so that the image looks as formatted above.

    double ydata[] = {1.9, 2.1, 3.5, 2.8, 3.99};
    cpl_vector *ycen = cpl_vector_wrap(5, ydata);

    // Run the main function to be tested
    cpl_test(res = cr2res_image_cut_rectify(img, ycen, 1));

    // What result should be
    int cmpdata[] = {9, 9, 9, 9, 9};
    cmp = cpl_image_wrap_int(5, 1, cmpdata);

    // Compare the two
    cpl_test_image_abs(res, cmp, 0);

    cpl_image_unwrap(img);
    cpl_image_unwrap(cmp);
    cpl_vector_unwrap(ycen);
    cpl_image_delete(res);

    return;
}

/*----------------------------------------------------------------------------*/
/**
  @brief
  @param
  @return
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_image_insert_rect(void)
{
    int recdata[] = {1, 2, 3, 2, 1,
                     1, 2, 9, 2, 9,
                     1, 9, 3, 9, 1,
                     9, 2, 3, 2, 1};
    cpl_image *rect_in = cpl_image_wrap_int(5, 4, recdata);
    cpl_image_flip(rect_in, 0);

    double ydata[] = {0.5, 1.1, 6.7, 11.9, 12.1};
    cpl_vector *ycen = cpl_vector_wrap(5, ydata);
    cpl_image *img_out = cpl_image_new(5, 12, CPL_TYPE_INT);
    int cmpdata[] = {0, 0, 0, 2, 9,
                     0, 0, 0, 2, 1,
                     0, 0, 0, 9, 1,
                     0, 0, 0, 2, 0,
                     0, 0, 0, 0, 0,
                     0, 0, 3, 0, 0,
                     0, 0, 9, 0, 0,
                     0, 0, 3, 0, 0,
                     0, 0, 3, 0, 0,
                     0, 0, 0, 0, 0,
                     0, 2, 0, 0, 0,
                     1, 2, 0, 0, 0};
    cpl_image *compare = cpl_image_wrap_int(5, 12, cmpdata);
    cpl_image_flip(compare, 0);

    cpl_test_zero(cr2res_image_insert_rect(rect_in, ycen, img_out));

    if (cpl_msg_get_level() == CPL_MSG_DEBUG)
    {
        cpl_image_save(img_out, "out.fits", CPL_TYPE_INT, NULL, CPL_IO_CREATE);
        cpl_image_save(compare, "cmp.fits", CPL_TYPE_INT, NULL, CPL_IO_CREATE);
    }

    // img_out == compare ?
    cpl_test_image_abs(img_out, compare, 0);

    cpl_image_unwrap(rect_in);
    cpl_image_unwrap(compare);
    cpl_vector_unwrap(ycen);
    cpl_image_delete(img_out);

    return;
}

/*----------------------------------------------------------------------------*/
/**
  @brief
  @param
  @return
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_polynomial_eval_vector(void)
{
    int i;
    double p0 = 1.1, p1 = 2.2, p2 = 3.3;
    double d, val;
    int n = 100;
    cpl_vector *in = cpl_vector_new(n);
    cpl_vector *out = cpl_vector_new(n);
    cpl_vector *res;
    cpl_polynomial *poly = cpl_polynomial_new(1);

    i = 0;
    cpl_polynomial_set_coeff(poly, (cpl_size *)&i, p0);
    i = 1;
    cpl_polynomial_set_coeff(poly, (cpl_size *)&i, p1);
    i = 2;
    cpl_polynomial_set_coeff(poly, (cpl_size *)&i, p2);

    for (i = 0; i < n; i++)
    {
        d = (double)i;
        val = d + (d / (n + 1));
        cpl_vector_set(in, i, d);
        val = (p2 * d * d) + (p1 * d) + p0;
        cpl_vector_set(out, i, val);
    }

    cpl_test(res = cr2res_polynomial_eval_vector(poly, in));

    cpl_test_vector_abs(res, out, DBL_EPSILON * n * n * 10);

    cpl_vector_delete(in);
    cpl_vector_delete(out);
    cpl_vector_delete(res);
    cpl_polynomial_delete(poly);

    return;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Find the regions with over-average values in a vector
  @param    invector    The vector to be analyzed
  @param    smooth      The size of the boxcar smoothing kernel
  @return   Vector derived as (invector - smoothed_vector - thresh),
            meaning that positive values are at least thresh larger than
            the smoothed vector.
            The returned vector needs to be deallocated by the caller.
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_threshold_spec(void)
{
    //define input
    int n = 10;
    double data[] = {1., 2., 1., 5., 3., 1., 15., 2., 0., 1.};
    cpl_vector *invector = cpl_vector_wrap(n, data);
    // expected data = data - median of boxcar - thresh
    // what is the expected behaviour at the borders?
    // -3, -3, -4, 0, -3, -4, 10, -2, -3, -1
    double outdata[] = {1-1-3, 2-1-3, 1-2-3, 5-3-3, 3-3-3, 1-3-3, 15-2-3, 2-2-3, 0-1-3, 1-1-3};
    cpl_vector *outvector = cpl_vector_wrap(n, outdata);

    //boxcar size = smooth + 3, for even values, and smooth + 2 for odd values
    int smooth = 0; //the documentation isn't really right about what smooth is
    double thresh = 3;
    //define output
    cpl_vector *res;

    //run test
    cpl_test(res = cr2res_threshold_spec(invector, smooth, thresh));

    //cpl_vector_dump(res, "test.log");
    //check output
    cpl_test_vector_abs(outvector, res, DBL_EPSILON * n * n * 10);

    //deallocate memory
    cpl_vector_unwrap(outvector);
    cpl_vector_unwrap(invector);
    cpl_vector_delete(res);

    return;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Find out the base name of a file (i.e. without prefix path)
  @param    filename    Full path name to scan.
  @return   Pointer to char within the input string.
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_get_base_name(void)
{
    //define input
    char *filename = "./../tests/cr2res_trace-test.log";
    char *res;

    //run test
    cpl_test(res = cr2res_get_base_name(filename));
    //test output
    cpl_test_eq_string(res, "cr2res_trace-test.log");
    //deallocate memory
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Find out the root part of a basename (name without extension).
  @param    filename    File name to scan.
  @return   Pointer to statically allocated string.
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_get_root_name(void)
{
    //define input
    //it only removes the extension for fits, dat, paf, txt, and ascii files
    char *filename = "cr2res_trace-test.fits";
    char *res;

    //run test
    cpl_test(res = cr2res_get_root_name(filename));
    //test output
    cpl_test_eq_string(res, "cr2res_trace-test");

    //deallocate memory
}

/*----------------------------------------------------------------------------*/
/**
   @brief   Extract the filename for the first frame of the given tag
   @param   in      A non-empty frameset
   @param   tag     The tag of the requested file
   @return  Pointer to the file
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_extract_filename(void)
{
    //define input
    cpl_frame *frame = cpl_frame_new();
    cpl_frame_set_filename(frame, "bla-test.log");
    cpl_frame_set_tag(frame, "test_correct");

    cpl_frame *other = cpl_frame_new();
    cpl_frame_set_filename(other, "blub-test.log");
    cpl_frame_set_tag(other, "test_wrong");

    cpl_frameset *in = cpl_frameset_new();
    cpl_frameset_insert(in, other);
    cpl_frameset_insert(in, frame);

    char *tag = "test_correct";
    const char *res;

    //run test
    cpl_test(res = cr2res_extract_filename(in, tag));
    //test output
    cpl_test_eq_string(res, "bla-test.log");

    //deallocate memory
    cpl_frameset_delete(in); //this should also delete the frames
}

/*----------------------------------------------------------------------------*/
/**
   @brief   Extract the frames with the given tag from a frameset
   @param   in      A non-empty frameset
   @param   tag     The tag of the requested frames
   @return  The newly created frameset or NULL on error

   The returned frameset must be de allocated with cpl_frameset_delete
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_extract_frameset(void)
{
    //define input
    cpl_frame *frame = cpl_frame_new();
    cpl_frame_set_filename(frame, "bla-test.log");
    cpl_frame_set_tag(frame, "test_correct");

    cpl_frame *other = cpl_frame_new();
    cpl_frame_set_filename(other, "blub-test.log");
    cpl_frame_set_tag(other, "test_wrong");

    cpl_frameset *in = cpl_frameset_new();
    cpl_frameset_insert(in, frame);
    cpl_frameset_insert(in, other);

    char *tag = "test_correct";
    cpl_frameset *res;

    //run test
    cpl_test(res = cr2res_extract_frameset(in, tag));
    //test output
    //test size
    cpl_test_eq(1, cpl_frameset_get_size(res));
    //check if filenames fit
    const char *fname1 = "bla-test.log";
    const char *fname2 = cpl_frame_get_filename(cpl_frameset_get_position(res, 0));
    cpl_test_eq_string(fname1, fname2); //Is that the right comparison?
    //check that the reference was copied as it is supposed to
    cpl_test_noneq_ptr(cpl_frameset_get_position(res, 0), frame);

    //deallocate memory
    //this also deletes the frames
    cpl_frameset_delete(res);
    cpl_frameset_delete(in);
}

/*----------------------------------------------------------------------------*/
/**
   @brief   Get the TRACE_WAVE table orders list
   @param   tab         A TRACE_WAVE table
   @param   nb_orders   The output array size
   @return  the array of orders or NULL in error case
    Needs to be deallocated with cpl_free
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_get_trace_table_orders(void)
{
    //define input
    int n = 10;
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    cpl_table *trace_wave = cpl_table_new(n);
    cpl_table_wrap_int(trace_wave, data, CR2RES_COL_ORDER); //what table do we need ?

    int cur_order = cpl_table_get(trace_wave, CR2RES_COL_ORDER, 5, NULL);

    cpl_test_eq(cur_order, 6);

    int nb_orders = 10;
    int *res;

    //run test
    cpl_test(res = cr2res_get_trace_table_orders(trace_wave, &nb_orders));
    cpl_test_eq(nb_orders, n);

    //test output
    //cpl_test_array_abs(res, data, DBL_EPSILON * n ); // This segfaults, unclear why.
    for (int i = 0; i < n; i++)
        cpl_test_eq(data[i], res[i]);

    //deallocate memory
    cpl_free(res);
    cpl_table_unwrap(trace_wave, CR2RES_COL_ORDER);
    cpl_table_delete(trace_wave);
}

/*----------------------------------------------------------------------------*/
/**
   @brief   Get the index in a TRACE_WAVE table
   @param   tab         A TRACE_WAVE table
   @param   order       the order number
   @param   trace_nb    the trace number
   @return  the row index or -1 in error case
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_get_trace_table_index(void)
{
    //define input
    int n = 10;
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int data2[] = {1, 1, 1, 1, 1, 1, 2, 1, 1, 1};
    cpl_table *trace_wave = cpl_table_new(n);
    cpl_table_wrap_int(trace_wave, data, CR2RES_COL_ORDER); //what table do we need ?
    cpl_table_wrap_int(trace_wave, data2, CR2RES_COL_TRACENB);

    int order = 5;
    int trace_nb = 1;
    cpl_size res;

    //run test
    cpl_test(res = cr2res_get_trace_table_index(trace_wave, order, trace_nb));
    //test output
    cpl_test_eq(res, 4);

    order = 7;
    // trace would be 2, but we just look for 1
    //run test
    cpl_test(res = cr2res_get_trace_table_index(trace_wave, order, trace_nb));
    //test output
    cpl_test_eq(res, -1);

    order = -10;
    // order does not exist
    //run test
    cpl_test(res = cr2res_get_trace_table_index(trace_wave, order, trace_nb));
    //test output
    cpl_test_eq(res, -1);

    //deallocate memory
    cpl_table_unwrap(trace_wave, CR2RES_COL_ORDER);
    cpl_table_unwrap(trace_wave, CR2RES_COL_TRACENB);
    cpl_table_delete(trace_wave);
}

/*----------------------------------------------------------------------------*/
/**
   @brief   Get the Wavelength polynomial from a TRACE_WAVE table
   @param   tab     A TRACE_WAVE table
   @param   poly_column CR2RES_COL_WAVELENGTH, CR2RES_COL_UPPER,
                        CR2RES_COL_LOWER or CR2RES_COL_ALL
   @return  The newly created polynomial or NULL in error case
   The returned object must be de allocated with cpl_polynomial_delete
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_get_trace_wave_poly(void)
{
    cpl_table *trace_wave = cpl_table_new(1);
    cpl_table_new_column_array(trace_wave, CR2RES_COL_WAVELENGTH, CPL_TYPE_DOUBLE, 3);
    cpl_table_new_column(trace_wave, CR2RES_COL_ORDER, CPL_TYPE_INT);
    cpl_table_new_column(trace_wave, CR2RES_COL_TRACENB, CPL_TYPE_INT);
    cpl_table_set(trace_wave, CR2RES_COL_ORDER, 0, 1);
    cpl_table_set(trace_wave, CR2RES_COL_TRACENB, 0, 1);
    double pdata[] = {1.1, 2.2, 3.3};
    cpl_array *parr = cpl_array_wrap_double(pdata, 3);
    cpl_table_set_array(trace_wave, CR2RES_COL_WAVELENGTH, 0, parr);

    //run test
    cpl_polynomial *res_poly;
    cpl_test(res_poly = cr2res_get_trace_wave_poly(trace_wave, CR2RES_COL_WAVELENGTH, 1, 1));
    //test output
    cpl_size power = 0;
    cpl_test_abs(1.1, cpl_polynomial_get_coeff(res_poly, &power), DBL_EPSILON);
    power = 1;
    cpl_test_abs(2.2, cpl_polynomial_get_coeff(res_poly, &power), DBL_EPSILON);
    power = 2;
    cpl_test_abs(3.3, cpl_polynomial_get_coeff(res_poly, &power), DBL_EPSILON);

    cpl_array_unwrap(parr);
    cpl_table_delete(trace_wave);
    cpl_polynomial_delete(res_poly);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Compute the polynomial from boundaries
  @param    wmin    First pixel wavelength
  @param    wmax    Last pixel wavelength
  @return   the polynomial or NULL in error case

  The returned polynomial must be deallocated with cpl_polynomial_delete
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_wlestimate_compute(void)
{
    //define input
    // these values return "simple" results
    double wmin = 2000;
    double wmax = 4047;
    cpl_polynomial *res;

    //run test
    cpl_test(res = cr2res_wlestimate_compute(wmin, wmax));
    //test output
    cpl_size power = 0;
    cpl_test_abs(1999.0, cpl_polynomial_get_coeff(res, &power), DBL_EPSILON);
    power = 1;
    cpl_test_abs(1.0, cpl_polynomial_get_coeff(res, &power), DBL_EPSILON);

    //deallocate memory
    cpl_polynomial_delete(res);

    wmin = 5000;
    wmax = 4047;
    cpl_test_null(cr2res_wlestimate_compute(wmin, wmax));

    wmin = -10;
    wmax = 0.11;
    cpl_test_null(cr2res_wlestimate_compute(wmin, wmax));
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Convert the order to the keyword index
  @param    order   Order (-49 to 50)
  @return   the order index or a negative value in error case
            (00 to 99)
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_convert_order_to_idx(void)
{
    //define input
    int order;

    //run test
    order = 50;
    cpl_test_eq(order, cr2res_convert_order_to_idx(order));

    order = -49;
    cpl_test_eq(order + 100, cr2res_convert_order_to_idx(order));

    order = 51;
    cpl_test_eq(-1, cr2res_convert_order_to_idx(order));

    order = -50;
    cpl_test_eq(-1, cr2res_convert_order_to_idx(order));
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Convert the keyword index to the order
  @param    order_idx   the order index (00 to 99)
  @return   Order (-50 to 50)
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_convert_idx_to_order(void)
{
    //define input
    int order;

    //run test
    order = 0;
    cpl_test_eq(order, cr2res_convert_idx_to_order(order));

    order = 51;
    cpl_test_eq(order - 100, cr2res_convert_idx_to_order(order));

    order = -1;
    cpl_test_eq(-1, cr2res_convert_idx_to_order(order));

    order = 99;
    cpl_test_eq(-1, cr2res_convert_idx_to_order(order));

    //deallocate memory
}

/*----------------------------------------------------------------------------*/
/**
   @brief   Convert an array to polynomial
   @param  	arr		An array
   @return  The newly created polynomial or NULL
   The returned object must be de allocated with cpl_polynomial_delete
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_convert_array_to_poly(void)
{
    //define input
    int n = 10;
    double data[] = {0.9, 1.5, 219.1, 123.8, 18, 123.3, 0.623, 0., 0.9, 1};
    cpl_array *arr = cpl_array_wrap_double(data, n);
    cpl_polynomial *res;
    cpl_size power = 0;
    double poly;

    //run test
    cpl_test(res = cr2res_convert_array_to_poly(arr));

    //test output
    for (int i = 0; i < n; i++)
    {
        power = i;
        poly = cpl_polynomial_get_coeff(res, &power);
        cpl_test_eq(data[i], poly);
    }

    //deallocate memory
    cpl_polynomial_delete(res);
    cpl_array_unwrap(arr);
}

/*----------------------------------------------------------------------------*/
/**
   @brief   Convert a  polynomial to array
   @param  	poly    A polynomial
   @param   size    The requested array size
   @return  The newly created array or NULL
   The returned object must be de allocated with cpl_array_delete
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_convert_poly_to_array(void)
{
    //define input
    int n = 10;
    cpl_array * res;
    double data[] = {0.9, 1.5, 219.1, 123.8, 18, 123.3, 0.623, 0., 0.9, 1};
    cpl_polynomial *poly = cpl_polynomial_new(1);
    for (cpl_size i = 0; i < n; i++)
        cpl_polynomial_set_coeff(poly, &i, data[i]);

    //also if size = NULL, no error is raised. Problem?

    //run test
    cpl_test(res = cr2res_convert_poly_to_array(poly, n));
    //test output
    for (int j = 0; j < n; j++)
        cpl_test_eq(cpl_array_get(res, j, NULL), data[j]);

    //deallocate memory
    cpl_polynomial_delete(poly);
    cpl_array_delete(res);
}

/*----------------------------------------------------------------------------*/
/**
  @brief   compute photon count error in [ADU]
  @param   ima_data in [ADU]
  @param   gain detector's gain in [e- / ADU]
  @param   ron  detector's read out noise in [ADU]
  @param   ima_errs output error image in [ADU]
  @return  cpl_error_code
  @note ima_errs need to be deallocated
        ima_data must contain the photon counts with no offsets
        this usually means the image must be overscan and bias corrected
        Then the shot noise can be calculated from the poissonian distribution
        as sqrt(electron-counts). To this (transformed back into ADUs) the
        readout noise is added in quadrature.
  @doc
  error is computed with standard formula

  \f$ err_{ADU} = \sqrt{ \frac{ counts }{ gain } + ron^{ 2 } } \f$

  If an image value is negative the associated error is set to RON
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_detector_shotnoise_model(void)
{
    //define input
    cpl_image *ima_data = cpl_image_new(5, 12, CPL_TYPE_INT);
    const double gain = 7;
    const double ron = 3;
    cpl_image *ima_errs;
    cpl_error_code res;
    int cmpdata[] = {3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3,
                     3, 3, 3, 3, 3};
    cpl_image *compare = cpl_image_wrap_int(5, 12, cmpdata);

    //run test
    cpl_test_eq(CPL_ERROR_NONE, cr2res_detector_shotnoise_model(ima_data, gain, ron, &ima_errs));
    //test output
    cpl_test_image_abs(ima_errs, compare, 0);

    //deallocate memory
    cpl_image_delete(ima_data);
    cpl_image_delete(ima_errs);
    cpl_image_unwrap(compare);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Get the pipeline copyright and license
  @return   The copyright and license string

  The function returns a pointer to the statically allocated license string.
  This string should not be modified using the returned pointer.
 */
/*----------------------------------------------------------------------------*/
static void test_cr2res_get_license(void)
{
    const char *license;
    cpl_test(license = cr2res_get_license());
    return;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Run the Unit tests
 */
/*----------------------------------------------------------------------------*/
int main(void)
{
    cpl_test_init(PACKAGE_BUGREPORT, CPL_MSG_DEBUG);

    test_cr2res_vector_get_rest();
    test_cr2res_vector_get_int();
    test_cr2res_polynomial_eval_vector();
    test_cr2res_image_cut_rectify();
    test_cr2res_image_insert_rect();
    test_cr2res_threshold_spec();
    test_cr2res_get_base_name();
    test_cr2res_get_root_name();
    test_cr2res_extract_frameset();
    test_cr2res_get_trace_table_orders();
    test_cr2res_get_trace_table_index();
    test_cr2res_get_trace_wave_poly();
    test_cr2res_wlestimate_compute();
    test_cr2res_convert_order_to_idx();
    test_cr2res_convert_idx_to_order();
    test_cr2res_convert_array_to_poly();
    test_cr2res_convert_poly_to_array();
    test_cr2res_detector_shotnoise_model();
    test_cr2res_get_license();

    return cpl_test_end(0);
}

/**@}*/

/*
* @Author: Dimitrios Katsaros
* @Date:   2015-04-24 14:23:43
* @Last Modified by:   Dimitrios Katsaros
* @Last Modified time: 2015-04-24 14:39:38
*/

#include <gst/check/gstcheck.h>

GST_START_TEST (test_<element_name>_<test_name1>)
{

}

GST_END_TEST;

static Suite *
<element_name>_suite (void)
{
  Suite *s = suite_create ("<element_name>");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_<element_name>_<test_name1>);

  return s;
}

GST_CHECK_MAIN (<element_name>);
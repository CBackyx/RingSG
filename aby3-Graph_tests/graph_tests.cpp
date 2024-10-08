#include "graph_tests.h"

oc::TestCollection graph_tests([](oc::TestCollection& tc) {
    tc.add("Sh3_Graph_eq_test;               ", Sh3_Graph_eq_test);
    tc.add("Sh3_Graph_multiplex_test;        ", Sh3_Graph_multiplex_test);
    tc.add("Sh3_Graph_compare_select_test;   ", Sh3_Graph_compare_select_test);
    tc.add("Sh3_Graph_OGA_test;              ", Sh3_Graph_OGA_test);
    tc.add("Sh3_Graph_CC_test;               ", Sh3_Graph_CC_test);
    tc.add("Sh3_Graph_Ours_test;             ", Sh3_Graph_Ours_test);
    tc.add("Sh3_Graph_shuffle_test;          ", Sh3_Graph_shuffle_test);
    tc.add("Sh3_Graph_reverse_shuffle_test;  ", Sh3_Graph_reverse_shuffle_test);
    tc.add("Sh3_Graph_sort_test;             ", Sh3_Graph_sort_test);
    tc.add("Sh3_Graph_PrefixAgg_test;        ", Sh3_Graph_PrefixAgg_test);
    tc.add("Sh3_Graph_GraphSC_test;          ", Sh3_Graph_GraphSC_test);
    tc.add("Sh3_Graph_CoGNN_test;            ", Sh3_Graph_CoGNN_test);
});
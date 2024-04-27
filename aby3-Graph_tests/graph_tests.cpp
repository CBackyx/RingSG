#include "graph_tests.h"

oc::TestCollection graph_tests([](oc::TestCollection& tc) {
    tc.add("Sh3_Graph_multiplex_test;        ", Sh3_Graph_multiplex_test);
    tc.add("Sh3_Graph_OGA_test;              ", Sh3_Graph_OGA_test);
    tc.add("Sh3_Graph_CC_test;               ", Sh3_Graph_CC_test);
});
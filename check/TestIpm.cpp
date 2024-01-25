#include <cmath>

#include "Highs.h"
#include "catch.hpp"

// I use dev_run to switch on/off printing and logging used for
// development of the unit test
const bool dev_run = true;
const double inf = kHighsInf;

TEST_CASE("test-analytic-centre", "[highs_ipm]") {
  //  std::string model = "greenbea.mps";
  //  std::string model = "adlittle.mps";
    std::string model = "afiro.mps"; 
  std::string filename = std::string(HIGHS_DIR) + "/check/instances/" + model;
  Highs highs;
  highs.setOptionValue("output_flag", dev_run);
  highs.readModel(filename);
  HighsLp lp = highs.getLp();
  lp.col_cost_.assign(lp.num_col_, 0);
  highs.passModel(lp);
  highs.setOptionValue("run_centring", true);
  highs.setOptionValue("solver", kIpmString);
  highs.setOptionValue("run_crossover", kHighsOffString);
  //  highs.setOptionValue("ipm_optimality_tolerance", 1e-2);
  HighsStatus run_status = highs.run();
  REQUIRE(run_status == HighsStatus::kOk);
}
TEST_CASE("test-analytic-centre-box", "[highs_ipm]") {
  Highs highs;
  const HighsInt dim = 2;
  HighsLp lp;
  lp.num_col_ = dim;
  lp.col_cost_.assign(dim, 0);
  lp.col_lower_.assign(dim, -1);
  lp.col_upper_.assign(dim, 1);
  highs.passModel(lp);

  std::vector<HighsInt> index = {0, 1};
  std::vector<double> value = {1, 1};

  const double root2 = std::sqrt(2.0);
  highs.addRow(-root2, root2, 2, index.data(), value.data());
  value[1] = -1;
  highs.addRow(-root2, root2, 2, index.data(), value.data());

  highs.setOptionValue("run_centring", true);
  highs.setOptionValue("solver", kIpmString);
  highs.setOptionValue("run_crossover", kHighsOffString);
  highs.setOptionValue("ipm_optimality_tolerance", 1e-2);
  HighsStatus run_status = highs.run();
  const HighsSolution& solution = highs.getSolution();
  for(HighsInt ix = 0; ix< dim; ix++)
    printf("Analytic centre component %d is %g\n", int(ix), solution.col_value[ix]);
}

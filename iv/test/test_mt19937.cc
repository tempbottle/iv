#include <gtest/gtest.h>
#include <ctime>
#include <iv/mt19937.h>
#include <iv/random_generator.h>

TEST(MT19937Case, Test) {
  iv::core::MT19937 g;
  for (int i = 0; i < 1000; ++i) {
    iv::core::MT19937::result_type res = g();
    ASSERT_LE(g.min(), res);
    ASSERT_GE(g.max(), res);
  }
}

TEST(MT19937Case, DistIntTest) {
  typedef iv::core::MT19937 engine_type;
  typedef iv::core::Uniform32RandomGenerator<engine_type, int> generator;
  generator gen(0, 100, std::time(nullptr));
  for (int i = 0; i < 10000000; ++i) {
    const int res = gen.get();
    ASSERT_LE(0, res);
    ASSERT_GE(100, res);
  }
}

TEST(MT19937Case, DistRealTest) {
  typedef iv::core::MT19937 engine_type;
  typedef iv::core::UniformRandomGenerator<engine_type> generator;
  generator gen(0.0, 1.0, std::time(nullptr));
  for (int i = 0; i < 10000000; ++i) {
    const double res = gen.get();
    ASSERT_LE(0.0, res);
    ASSERT_GT(1.0, res);
  }
}

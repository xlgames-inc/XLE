/* -*- C++ -*- ------------------------------------------------------------
 @@COPYRIGHT@@
 *-----------------------------------------------------------------------*/
/** @file
 */

#ifndef __CML_MATRIX_MATRIX_PRODUCT_TPP
#error "matrix/matrix_product.tpp not included correctly"
#endif

#include <cml/matrix/detail/resize.h>

namespace cml {

template<class Sub1, class Sub2,
  enable_if_matrix_t<Sub1>*, enable_if_matrix_t<Sub2>*>
inline auto operator*(Sub1&& sub1, Sub2&& sub2)
-> matrix_inner_product_promote_t<
  actual_operand_type_of_t<decltype(sub1)>,
  actual_operand_type_of_t<decltype(sub2)>>
{
  typedef matrix_inner_product_promote_t<
    actual_operand_type_of_t<decltype(sub1)>,
    actual_operand_type_of_t<decltype(sub2)>>		result_type;

  cml::check_same_inner_size(sub1, sub2);

  result_type M;
  detail::resize(M, array_rows_of(sub1), array_cols_of(sub2));
  for(int i = 0; i < M.rows(); ++ i) {
    for(int j = 0; j < M.cols(); ++ j) {
      auto m = sub1(i,0) * sub2(0,j);
      for(int k = 1; k < sub1.cols(); ++ k) m += sub1(i,k) * sub2(k,j);
      M(i,j) = m;
    }
  }
  return M;
}

} // namespace cml

// -------------------------------------------------------------------------
// vim:ft=cpp:sw=2

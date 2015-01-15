/* -*- C++ -*- ------------------------------------------------------------
 @@COPYRIGHT@@
 *-----------------------------------------------------------------------*/
/** @file
 *  @brief A set of very simple examples of current CML functionality.
 */

#include "cml_config.h"         // Must be first (for now)!

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cml/cml.h>
using namespace cml;

using std::cout;
using std::endl;
using std::sqrt;

void example1()
{
    cout << std::endl << "Example1:" << endl;

    /* 3-space column vector, fixed length, double coordinates: */
    typedef cml::vector< double, fixed<3> > vector_d3;

    vector_d3 u(0.,0.,1.), v;
    v[0] = 1.; v[1] = 0.; v[2] = 0.;

    cout << "  u = " << u << endl;
    cout << "  v = " << v << endl;
    cout << "  length(u) = " << length(u) << endl;
    cout << "  normalize(v) = " << normalize(v) << endl;

    cout << "  dot(u,v) = " << dot(u,v) << endl;
    cout << "  dot(u,u) = " << dot(u,u) << endl;
    cout << "  dot(u+v,v) = " << dot(u+v,v) << endl;
    cout << "  cos(u,v) = " << dot(u,v)/sqrt(dot(u,u)*dot(v,v))
        << endl;

    cout << "  (u+v).normalize() = " << (u+v).normalize() << endl;
    cout << "  normalize(u+v) = " << normalize(u+v) << endl;
    cout << "  (-u).normalize() = " << (-u).normalize() << endl;
    cout << "  (-(u+v)).normalize() = " << (-(u+v)).normalize() << endl;
}

void example2()
{
    cout << std::endl << "Example2:" << endl;

    /* 3-space column vector, dynamic length, double coordinates: */
    typedef cml::vector< double, dynamic<> > vector_d;

    vector_d u(0.,0.,1.);
    vector_d v(3);

    v[0] = 1.; v[1] = 0.; v[2] = 0.;

    cout << "  dot(u,v) = " << dot(u,v) << endl;
    cout << "  dot(u,u) = " << dot(u,u) << endl;
    cout << "  dot(u+v,v) = " << dot(u+v,v) << endl;
    cout << "  cos(u,v) = " << dot(u,v)/sqrt(dot(u,u)*dot(v,v))
        << endl;
}

void example3()
{
    cout << std::endl << "Example3:" << endl;

    /* 3-space matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, fixed<3,3>, col_basis> matrix_d3;

    matrix_d3 A(
            1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0
            );
    matrix_d3 B, C;

    B(0,0) = 1.0; B(0,1) = 0.0; B(0,2) = 1.0;
    B(1,0) = 0.0; B(1,1) = 1.0; B(1,2) = 0.0;
    B(2,0) = 0.0; B(2,1) = 0.0; B(2,2) = 1.0;

    cout << "  A = " << A << endl;
    cout << "  B = " << B << endl;

    C = T(A+B);
    cout << "  C(0,0) = " << C(0,0) << endl;
    cout << "  C(2,0) = " << C(2,0) << endl;

    A = identity(C);
    cout << "  identity(C) = " << A << endl;
}

void example4()
{
    cout << std::endl << "Example4:" << endl;

    /* 3-space matrix, fixed size, double coordinates: */
    typedef cml::matrix<double, fixed<3,3>, col_basis> matrix_d3;

    /* 3-space matrix, dynamic size, double coordinates: */
    typedef cml::matrix<double, dynamic<>, col_basis> matrix_d;

    matrix_d3 A;
    matrix_d B(
            1.0, 0.0, 1.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0
            );
    matrix_d C(3,3);
    C.zero();
    cout << "  C is " << C.rows() << "x" << C.cols() << endl;

    A(0,0) = 1.0; A(0,1) = 0.0; A(0,2) = 0.0;
    A(1,0) = 0.0; A(1,1) = 1.0; A(1,2) = 0.0;
    A(2,0) = 0.0; A(2,1) = 0.0; A(2,2) = 1.0;

    cout << "  A = " << A << endl;
    cout << "  B = " << B << endl;

    C = transpose(A+B);
    cout << "  C(0,0) = " << C(0,0) << endl;

    A = identity(C);
    cout << "  identity(C) = " << A << endl;
}

void example5()
{
    cout << std::endl << "Example5:" << endl;

    /* 3-space matrix, fixed size, double coordinates: */
    typedef cml::matrix<double, fixed<3,3>, col_basis> matrix_d3;

    /* 3-space matrix, dynamic size, double coordinates: */
    typedef cml::matrix<double, dynamic<>, col_basis> matrix_d;

    matrix_d3 A(
            1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0);
    matrix_d3 B(
            1.0, 0.0, 1.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0
            );
    matrix_d C(3,3);

    C = A+B;
    cout << "  C(0,0) = " << C(0,0) << endl;
    cout << C << endl;

    C = A+T(B);
    cout << C << endl;
}

void example6()
{
    cout << std::endl << "Example6:" << endl;

    /* 3-space matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, fixed<3,3>, col_basis> matrix_d3;

    /* Column vector of the matrix: */
    typedef matrix_d3::col_vector_type cvector_d3;

    matrix_d3 A, B, C;

    A(0,0) = 1.0; A(0,1) = 0.0; A(0,2) = 1.0;
    A(1,0) = 0.0; A(1,1) = 1.0; A(1,2) = 0.0;
    A(2,0) = 0.0; A(2,1) = 0.0; A(2,2) = 1.0;

    B(0,0) = 1.0; B(0,1) = 0.0; B(0,2) = 1.0;
    B(1,0) = 0.0; B(1,1) = 1.0; B(1,2) = 0.0;
    B(2,0) = 0.0; B(2,1) = 0.0; B(2,2) = 1.0;

    C = T(A)+B;
    cout << "  T(A)+B = " << C << endl;

    cvector_d3 v = col(C,0);
    cout << "  C(0) = " << endl << v << endl;
    v = col(C,1);
    cout << "  C(1) = " << endl << v << endl;
    v = col(C,2);
    cout << "  C(2) = " << endl << v << endl;

    v = col(T(A)+B,2);
    cout << "  (T(A)+B)(2) = " << endl << v << endl;
}

void example7()
{
    cout << std::endl << "Example7:" << endl;

    /* 3-space matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, fixed<3,3>, col_basis> matrix_d3;

    matrix_d3 A, B, C;

    A(0,0) = 1.0; A(0,1) = 0.0; A(0,2) = 1.0;
    A(1,0) = 0.0; A(1,1) = 1.0; A(1,2) = 0.0;
    A(2,0) = 0.0; A(2,1) = 0.0; A(2,2) = 1.0;

    B(0,0) = 1.0; B(0,1) = 0.0; B(0,2) = 1.0;
    B(1,0) = 0.0; B(1,1) = 1.0; B(1,2) = 0.0;
    B(2,0) = 0.0; B(2,1) = 0.0; B(2,2) = 1.0;

    C = A*B;
    cout << "  A = " << A << endl;
    cout << "  B = " << B << endl;
    cout << "  A*B = " << C << endl;
}

void example8()
{
    cout << std::endl << "Example8:" << endl;

    /* 3-space column vector, fixed length, double coordinates: */
    typedef cml::vector< double, external<3> > vector_d3;

    double c_u[3] = {0.,0.,1.};
    double c_v[3] = {1.,0.,0.};
    vector_d3 u(c_u), v(c_v);

    cout << "  u = " << u << endl;
    cout << "  v = " << v << endl;
    cout << "  dot(u,v) = " << dot(u,v) << endl;
    cout << "  dot(u,u) = " << dot(u,u) << endl;
    cout << "  dot(v,v) = " << dot(v,v) << endl;
    cout << "  dot(u+v,v) = " << dot(u+v,v) << endl;
    cout << "  cos(u,v) = " << dot(u,v)/sqrt(dot(u,u)*dot(v,v))
        << endl;
}

#if defined(CML_ENABLE_DOT_OPERATOR)
void example9()
{
    cout << std::endl << "Example9:" << endl;

    /* 3-space column vector, fixed length, double coordinates: */
    typedef cml::vector< double, fixed<3> > vector_d3;

    vector_d3 u, v;
    u[0] = 0.; u[1] = 0.; u[2] = 1.;
    v[0] = 1.; v[1] = 0.; v[2] = 0.;

    cout << "  u = " << u << endl;
    cout << "  v = " << v << endl;

    cout << "  dot(u,v) = " << u*v << endl;
    cout << "  dot(u,u) = " << u*u << endl;
    cout << "  dot(u+v,v) = " << (u+v)*v << endl;
    cout << "  cos(u,v) = " << (u*v)/sqrt((u*u)*(v*v)) << endl;
}
#endif

void example10()
{
    cout << std::endl << "Example10:" << endl;

    /* 4x3 matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, fixed<4,3>, col_basis, col_major> matrix_d43;

    /* 3-space vector, fixed length, double coordinates: */
    typedef cml::vector< double, fixed<3> > vector_d3;

    /* 4-space vector, fixed length, double coordinates: */
    typedef cml::vector< double, fixed<4> > vector_d4;

    matrix_d43 A;

    A(0,0) = 1.0; A(0,1) = 1.0; A(0,2) = 1.0;
    A(1,0) = 0.0; A(1,1) = 1.0; A(1,2) = 1.0;
    A(2,0) = 0.0; A(2,1) = 0.0; A(2,2) = 1.0;
    A(3,0) = 0.0; A(3,1) = 0.0; A(3,2) = 1.0;

    vector_d3 x(1.,0.,1.);

    cout << "A = " << A << endl;
    cout << "x = " << x << endl;

    vector_d4 y = A*x;
    cout << "y = A*x = " << y << endl;

    vector_d3 yp = y*A;
    cout << "yp = mul(y,A) = " << yp << endl;

    vector_d4 ypp = A*yp;
    cout << "ypp = mul(A,yp) = " << ypp << endl;

    matrix_d43::transposed_type B = T(A);
    cout << "T(A) = " << B << endl;

    vector_d4 z = yp*B;
    cout << "z = yp*T(A) = " << z << endl;
}

void example11()
{
    cout << std::endl << "Example11:" << endl;

    /* 4x3 matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, fixed<4,3>, col_basis, col_major> matrix_d43;

    /* 3-space vector, fixed length, double coordinates: */
    typedef cml::vector< double, fixed<3> > vector_d3;

    /* 4-space vector, fixed length, double coordinates: */
    typedef cml::vector< double, fixed<4> > vector_d4;

    matrix_d43 A;

    A(0,0) = 1.0; A(0,1) = 1.0; A(0,2) = 1.0;
    A(1,0) = 0.0; A(1,1) = 1.0; A(1,2) = 1.0;
    A(2,0) = 0.0; A(2,1) = 0.0; A(2,2) = 1.0;
    A(3,0) = 0.0; A(3,1) = 0.0; A(3,2) = 1.0;

    vector_d3 x;
    x[0] = 1.; x[1] = 0.; x[2] = 1.;

    cout << "A = " << A << endl;
    cout << "x = " << x << endl;

    vector_d4 y = A*x;
    cout << "y = A*x = " << y << endl;

    matrix_d43 B = outer(y,x);
    cout << "B = outer(y,x) = " << B << endl;
}

void example12()
{
    /* 4x3 matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, fixed<4,3>, col_basis, col_major> matrix_d43;

    /* 3-space matrix, fixed length, float coordinates: */
    typedef cml::matrix<float, fixed<3,3>, col_basis, col_major> matrix_f33;

    /* 3-space vector, fixed length, double coordinates: */
    typedef cml::vector< double, fixed<3> > vector_d3;

    /* 4-space vector, fixed length, double coordinates: */
    typedef cml::vector< float, fixed<4> > vector_d4;

    cout << std::endl << "Example12:" << endl;

    matrix_d43 A;

    A(0,0) = 1.0; A(0,1) = 1.0; A(0,2) = 1.0;
    A(1,0) = 0.0; A(1,1) = 1.0; A(1,2) = 1.0;
    A(2,0) = 0.0; A(2,1) = 0.0; A(2,2) = 1.0;
    A(3,0) = 0.0; A(3,1) = 0.0; A(3,2) = 1.0;

    matrix_f33 B = T(A)*A;

    cout << "A = " << A << endl;
    cout << "B[f33] = T(A)*A = " << B << endl;
}

void example13()
{
    cout << std::endl << "Example13:" << endl;

    /* 4x4 matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, fixed<4,4>, col_basis, col_major> matrix_d44;

    matrix_d44 M,A;
    M(0,0) = 1.0; M(0,1) = 1.0; M(0,2) = 1.0; M(0,3) = 1.0;
    M(1,0) = 0.0; M(1,1) = 1.0; M(1,2) = 1.0; M(1,3) = 1.0;
    M(2,0) = 0.0; M(2,1) = 0.0; M(2,2) = 1.0; M(2,3) = 1.0;
    M(3,0) = 0.0; M(3,1) = 0.0; M(3,2) = 0.0; M(3,3) = 1.0;

    cout << "M = " << M << endl;
    cout << "M = LU = " << cml::lu(M) << endl;
    cout << "det(M) = " << determinant(M) << endl;

    A(0,0) = 1.0; A(0,1) = 0.0; A(0,2) = 0.0; A(0,3) = 0.0;
    A(1,0) = 0.0; A(1,1) = 1.0; A(1,2) = 0.0; A(1,3) = 0.0;
    A(2,0) = 0.0; A(2,1) = 0.0; A(2,2) = 1.0; A(2,3) = 0.0;
    A(3,0) = 0.0; A(3,1) = 0.0; A(3,2) = 0.0; A(3,3) = 1.0;

    matrix_d44 D = M+A;
    cout << "M+A = " << D << endl;
    cout << "M+A = LU = " << cml::lu(M+A) << endl;
    cout << "det(M+A) = " << determinant(M+A) << endl;
}

void example14()
{
    cout << std::endl << "Example14:" << endl;

    /* 4x4 matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, dynamic<>, col_basis, col_major> matrix_d44;
    typedef matrix_d44::col_vector_type vector_d4;

    matrix_d44 M(4,4);

    M(0,0) = 1.0;  M(0,1) = 8.0;  M(0,2) = 3.0; M(0,3) = 11.0;
    M(1,0) = 9.0;  M(1,1) = 5.0;  M(1,2) = 2.0; M(1,3) = 17.0;
    M(2,0) = 12.0; M(2,1) = 12.0; M(2,2) = 7.0; M(2,3) = 2.0;
    M(3,0) = 15.0; M(3,1) = 17.0; M(3,2) = 9.0; M(3,3) = 16.0;

    matrix_d44 LU(4,4); LU = cml::lu(M);
    cout << "M = " << M << endl;
    cout << "LU(M) = " << LU << endl;

    vector_d4 y(4), x(4);
    y[0] = 1.; y[1] = 7.; y[2] = 13.; y[3] = 6.;
    cout << "y = " << y << endl;

    x = cml::lu_solve(LU,y);
    cout << "x = lu_solve(M,y) = " << x << endl;

    y = M*x;
    cout << "y = M*x = " << y << endl;
}

void example15()
{
    cout << std::endl << "Example15:" << endl;

    /* 4x4 matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, fixed<4,4>, col_basis, col_major> matrix_d44;
    typedef matrix_d44::col_vector_type vector_d4;

    matrix_d44 M;
    M(0,0) = 1.0;  M(0,1) = 8.0;  M(0,2) = 3.0; M(0,3) = 11.0;
    M(1,0) = 9.0;  M(1,1) = 5.0;  M(1,2) = 2.0; M(1,3) = 17.0;
    M(2,0) = 12.0; M(2,1) = 12.0; M(2,2) = 7.0; M(2,3) = 2.0;
    M(3,0) = 15.0; M(3,1) = 17.0; M(3,2) = 9.0; M(3,3) = 16.0;

    matrix_d44 LU = cml::lu(M);
    cout << "M = " << M << endl;
    cout << "M = LU = " << LU << endl;

    vector_d4 x, y;
    y[0] = 1.; y[1] = 7.; y[2] = 13.; y[3] = 6.;
    cout << "y = " << y << endl;

    matrix_d44 Minv = inverse(M);
    cout << "Minv = " << Minv << endl;

    x = Minv*y;
    y = M*x;
    cout << "x = Minv*y = " << x << endl;
    cout << "y = M*x = " << y << endl;

    double D = determinant(M);
    cout << "det(M) = " << D << endl;
    cout << "det(M)[lu] = "
        << cml::detail::determinant_f<matrix_d44,0>()(M) << endl;
}

void example16()
{
    cout << std::endl << "Example16:" << endl;

    /* 4x4 matrix, dynamic length, double coordinates: */
    typedef cml::matrix<double, dynamic<>, col_basis, col_major> matrix_d44;
    typedef matrix_d44::col_vector_type vector_d4;

    matrix_d44 M(4,4);
    M(0,0) = 1.0;  M(0,1) = 8.0;  M(0,2) = 3.0; M(0,3) = 11.0;
    M(1,0) = 9.0;  M(1,1) = 5.0;  M(1,2) = 2.0; M(1,3) = 17.0;
    M(2,0) = 12.0; M(2,1) = 12.0; M(2,2) = 7.0; M(2,3) = 2.0;
    M(3,0) = 15.0; M(3,1) = 17.0; M(3,2) = 9.0; M(3,3) = 16.0;

    matrix_d44 Minv(4,4); Minv = inverse(M);
    cout << "M^-1 = " << Minv << endl;

    cout << "M^-1[lu] = "
        << cml::detail::inverse_f<matrix_d44,0>()(M) << endl;

    vector_d4 x(4), y(4);
    y[0] = 1.; y[1] = 7.; y[2] = 13.; y[3] = 6.;
    cout << "y = " << y << endl;

    x = Minv*y;
    cout << "x = Minv*y = " << x << endl;

    y = M*x;
    cout << "y = M*x = " << y << endl;
}

void example17()
{
    cout << std::endl << "Example17:" << endl;

    /* 4x4 matrix, dynamic length, double coordinates: */
    typedef cml::matrix<double, fixed<4,4>, col_basis, col_major> matrix_d44;
    typedef cml::vector< double, fixed<4> > vector_d4;

    /* XXX This currently doesn't work for dynamic-size matrices: */
    double _M[4][4] = {
        { 1.0,  8.0,  3.0, 11.0 },
        { 9.0,  5.0,  2.0, 17.0 },
        { 12.0, 12.0, 7.0, 2.0  },
        { 15.0, 17.0, 9.0, 16.0 }
    };
    matrix_d44 M(_M);

    matrix_d44 Minv = inverse(M);
    cout << "M^-1 = " << Minv << endl;
    cout << "M^-1[lu] = "
        << cml::detail::inverse_f<matrix_d44,0>()(M) << endl;

    double _y[] = {1.,7.,13.,6.};
    vector_d4 x, y(_y);
    cout << "y = " << y << endl;

    x = Minv*y;
    y = M*x;
    cout << "x = Minv*y = " << x << endl;
    cout << "y = M*x = " << y << endl;
}

void example18()
{
    cout << std::endl << "Example18:" << endl;

    typedef quaternion<double, fixed<>, scalar_first> quaternion_type;

    double v1[] = {1.,0.,0.};
    quaternion_type p(1.,v1), q(0., 1., 0., 1.);
    quaternion_type r, s;
    cout << "p @ " << p.data() << endl;
    cout << "q @ " << q.data() << endl;
    cout << "p = " << p << endl;
    cout << "q = " << q << endl;
    
    r = conjugate(p);
    cout << "r = conjugate(p) = " << r << endl;

    r = conjugate(q);
    cout << "r = conjugate(q) = " << r << endl;

    r = p + q;
    cout << "r = p+q = " << r << endl;

    r = p + conjugate(q);
    cout << "r = p + conjugate(q) = " << r << endl;

    r = p + conjugate(2.*q);
    cout << "r = p + conjugate(2*q) = " << r << endl;

    r = p + conjugate(q)*2.;
    cout << "r = p+ conjugate(q)*2 = " << r << endl;

    r = p*q;
    cout << "r = p*q = " << r << endl;

    r = p*p;
    cout << "r = p*p = " << r << endl;

    r = p*conjugate(p);
    cout << "r = p*~p = " << r << endl;

    r = (conjugate(p))/real(p*conjugate(p));
    cout << "r = conjugate(p)/real(p*conjugate(p)) = " << r << endl;

    s = r*p;
    cout << "s = r*p = " << s << endl;

    r = conjugate(p)/norm(p);
    cout << "r = conjugate(p)/norm(p) = " << r << endl;

    s = r*p;
    cout << "s = r*p = " << s << endl;

    r = inverse(p);
    cout << "r = inverse(p) = " << r << endl;

    s = r*p;
    cout << "s = r*p = " << s << endl;
}

void example19()
{
    cout << std::endl << "Example19:" << endl;

    typedef negative_cross cross_type;
    typedef quaternion<
        double, fixed<>, scalar_first, cross_type> quaternion_type;

    double v1[] = {1.,0.,0.}, v2[] = {0.,1.,0.};
    quaternion_type p(0.,v1), q(0.,v2), r, s;
    cout << "p = " << p << endl;
    cout << "q = " << q << endl;
    
    r = p*p;
    cout << "r = p*p = " << r << endl;

    r = q*q;
    cout << "r = q*q = " << r << endl;

    r = p*conjugate(p);
    cout << "r = p*~p = " << r << endl;

    r = (conjugate(p))/real(p*conjugate(p));
    cout << "r = conjugate(p)/real(p*conjugate(p)) = " << r << endl;
    
    r = inverse(p);
    cout << "r = inverse(p) = " << r << endl;

    s = r*p;
    cout << "s = r*p = " << s << endl;

    r = p*q;
    cout << "r = p*q = p_v ^ q_v = " << r << endl;

    r = q*p;
    cout << "r = p*q = q_v ^ p_v = " << r << endl;
}

void example20()
{
    cout << std::endl << "Example20:" << endl;

    typedef positive_cross cross_type;
    typedef quaternion<
        double, fixed<>, scalar_first, cross_type> quaternion_type;
    typedef quaternion<
        double, external<>, scalar_first, cross_type> extquat_type;

    double _v1[4] = {0.,3.,4.,0.};
    extquat_type p(_v1);

    double v2[] = {11.,60.,0.};
    quaternion_type q(0.,v2), r, s;
    cout << "p = " << p << endl;
    cout << "length(p) = " << length(p) << endl;
    cout << "q = " << q << endl;
    cout << "length(q) = " << length(q) << endl;
    
    r = p / length(p);
    cout << "r = p / length(p) = " << r << endl;

    s = p;
    p.normalize();
    cout << "p.normalize() = " << r << endl;
    p = s;

    cout << "(-p) = " << quaternion_type(-p) << endl;
    cout << "(-p).length() = " << (-p).length() << endl;
    cout << "length(-p) = " << length(-p) << endl;

    cout << "conjugate(p) = " << quaternion_type(conjugate(p)) << endl;
    cout << "conjugate(p).length() = " << conjugate(p).length() << endl;
    cout << "length(conjugate(p)) = " << length(conjugate(p)) << endl;

    cout << "inverse(p) = " << quaternion_type(inverse(p)) << endl;
    cout << "inverse(p).length() = " << inverse(p).length() << endl;
    cout << "length(inverse(p)) = " << length(inverse(p)) << endl;

    s = p+q;
    cout << "s = p+q = " << s << endl;
    cout << "length(s) = " << length(s) << endl;
    cout << "normalize(s) = " << normalize(s) << endl;

    r = normalize(p+q);
    cout << "length(p+q) = " << length(p+q) << endl;
    cout << "r = normalize(p+q) = " << r << endl;

    cout << "length(p*q) = " << length(p*q) << endl;
    cout << "(p*q).length() = " << (p*q).length() << endl;

    identity(r);
    cout << "identity(r) = " << r << endl;

    s = r*p;
    cout << "s = r*p = " << s << endl;
}

void example21()
{
    cout << std::endl << "Example21:" << endl;

    /* 3-space matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, fixed<3,3>, col_basis> matrix_d3;

    matrix_d3 A, B, C, D;

    A(0,0) = 1.0; A(0,1) = 0.0; A(0,2) = 1.0;
    A(1,0) = 0.0; A(1,1) = 1.0; A(1,2) = 0.0;
    A(2,0) = 0.0; A(2,1) = 0.0; A(2,2) = 1.0;

    B(0,0) = 1.0; B(0,1) = 0.0; B(0,2) = 1.0;
    B(1,0) = 0.0; B(1,1) = 1.0; B(1,2) = 0.0;
    B(2,0) = 0.0; B(2,1) = 0.0; B(2,2) = 1.0;

    C = A*B;
    cout << "  A = " << A << endl;
    cout << "  B = " << B << endl;
    cout << "  C = A*B = " << C << endl;

    D = C*A;
    cout << "  C*A = " << D << endl;

    C *= A;
    cout << "  C*=A = " << C << endl;

    C *= A;
    cout << "  C = C*A = " << C << endl;
}

void example22()
{
    typedef constants<double> constants_t;

    cout << std::endl << "Example22:" << endl;
    cout << std::setprecision(16) << endl;
    cout << "  pi = " << constants_t::pi() << endl;
    cout << "  two_pi = " << constants_t::two_pi() << endl;
    cout << "  sqrt(3) = " << constants_t::sqrt_3() << endl;
    cout << "  e = " << constants_t::e() << endl;
}

#if 0
void example23()
{
    using cml::lerp;

    cout << std::endl << "Example23:" << endl;

    double f0 = 0., f1 = 1., u = .3;
    double fu = lerp(f0,f1,u);
    cout << "  f(0) = " << f0 << "  f(1) = " << f1 << ":" << endl;
    cout << "    f(.3) ~= lerp(" << f0 << "," << f1 << "," << u << ") = "
        << fu << endl;
}
#endif


typedef vector< double, fixed<3> > g_vector_t;
void example24_part1(const g_vector_t& v)
{
    cout << "I got " << v << endl;
}

void example24()
{
    cout << std::endl << "Example24:" << endl;
    g_vector_t u(1., 1., 0.), v(1., 0., 0.);
    example24_part1(u+2.*v);
}

void example25()
{
    cout << std::endl << "Example25:" << endl;

    /* 3-space matrix, fixed length, double coordinates, col basis: */
    typedef cml::matrix< double, fixed<3,3> > matrix_d3c;

    /* 3-space matrix, fixed length, double coordinates: */
    typedef cml::matrix<double, fixed<3,3>, row_basis> matrix_d3r;

    /* 3-space vector, fixed length, double coordinates: */
    typedef cml::vector< double, fixed<3> > vector_d3;

    matrix_d3c A(
            1., 0., 0.,
            0., 1., 0.,
            0., 0., 1.
            );
    cout << "A:\n" << A << endl;

    vector_d3 x(1., 0., 1.);
    vector_d3 y = A*x;

    matrix_d3r B = outer(x,y);

    cout << "B:\n" << B << endl;

    A = 2.*B;
    cout << "A = 2*B =\n" << A << endl;
}

void example26()
{
    cout << std::endl << "Example26:" << endl;

    typedef quaternion<double, fixed<>, scalar_first> quaternion_type;

    double v1[] = {1.,0.,0.};
    quaternion_type p(1.,v1), q(0., 1., 0., 1.);
    quaternion_type r = p;
    cout << "p = " << p << endl;
    cout << "q = " << q << endl;

    cout << "p * q = " << p*q << endl;
    p *= q;
    cout << "p *= q = " << p << endl;

    cout << "p * (p+q) = " << p*(p+q) << endl;
    p *= (p+q);
    cout << "p *= (p+q) = " << p << endl;
}

void example27()
{
    cout << std::endl << "Example27:" << endl;

    /* Shorthand: */
    typedef unsigned long long ULL;

    /* 3-space matrix, fixed length, unsigned long-long coordinates: */
    typedef cml::matrix< ULL, fixed<3,3> > matrix_ull3;

    matrix_ull3 A(
            1, 0, 0,
            0, 1, 0,
            0, 0, 1
            );
    cout << "A:\n" << A << endl;

    cml::vector3d x(1., 0., 1.);
    cml::vector3d y = A*x;

    cml::vector< ULL, fixed<3> > x2(1, 2, 3);
    matrix_ull3 B = outer(x2,x2);

    cout << "B:\n" << B << endl;

    A = 2*B;
    cout << "A = 2*B =\n" << A << endl;
}

void example28()
{
    float degf = cml::deg(1.f);
    float radf = cml::rad(degf);
    double degd = cml::deg(1.);
    double radd = cml::rad(degd);
}


int main()
{
    example1();
    example2();
    example3();
    example4();
    example5();
    example6();
    example7();
    example8();
#if defined(CML_ENABLE_DOT_OPERATOR)
    example9();
#endif
    example10();
    example11();
    example12();
    example13();
    example14();
    example15();
    example16();
    example17();
    example18();
    example19();
    example20();
    example21();
    example22();
//    example23();
    example24();
    example25();
    example26();
    example27();
    return 0;
}

// -------------------------------------------------------------------------
// vim:ft=cpp


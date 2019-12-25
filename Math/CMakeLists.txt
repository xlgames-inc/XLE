set(Src 
    EigenVector.cpp
    Geometry.cpp
    Interpolation.cpp
    Matrix.cpp
    Noise.cpp
    PoissonSolver.cpp
    ProjectionMath.cpp
    RectanglePacking.cpp
    RegularNumberField.cpp
    StraightSkeleton.cpp
    Transformations.cpp)
set(Headers 
    EigenVector.h
    Geometry.h
    Interpolation.h
    Matrix.h
    Noise.h
    PoissonSolverDetail.h
    PoissonSolver.h
    ProjectionMath.h
    Quaternion.h
    RectanglePacking.h
    RegularNumberField.h
    StraightSkeleton.h
    Transformations.h
    Vector.h
    XLEMath.h)

BasicLibrary(Math "${Src}" "${Headers}")

target_link_libraries(Math ForeignMisc)  # required to pull in interface include directories (etc)

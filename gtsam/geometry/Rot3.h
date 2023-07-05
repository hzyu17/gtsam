/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    Rot3.h
 * @brief   3D rotation represented as a rotation matrix or quaternion
 * @author  Alireza Fathi
 * @author  Christian Potthast
 * @author  Frank Dellaert
 * @author  Richard Roberts
 * @author  Luca Carlone
 */
// \callgraph

#pragma once

#include <gtsam/geometry/Unit3.h>
#include <gtsam/geometry/Quaternion.h>
#include <gtsam/geometry/SO3.h>
#include <gtsam/base/concepts.h>
#include <gtsam/config.h> // Get GTSAM_USE_QUATERNIONS macro

// You can override the default coordinate mode using this flag
#ifndef ROT3_DEFAULT_COORDINATES_MODE
  #ifdef GTSAM_USE_QUATERNIONS
    // Exponential map is very cheap for quaternions
    #define ROT3_DEFAULT_COORDINATES_MODE Rot3::EXPMAP
  #else
    // If user doesn't require GTSAM_ROT3_EXPMAP in cmake when building
    #ifndef GTSAM_ROT3_EXPMAP
      // For rotation matrices, the Cayley transform is a fast retract alternative
      #define ROT3_DEFAULT_COORDINATES_MODE Rot3::CAYLEY
    #else
      #define ROT3_DEFAULT_COORDINATES_MODE Rot3::EXPMAP
    #endif
  #endif
#endif

namespace gtsam {

  /**
   * @brief A 3D rotation represented as a rotation matrix if the preprocessor
   * symbol GTSAM_USE_QUATERNIONS is not defined, or as a quaternion if it
   * is defined.
   * @addtogroup geometry
   * \nosubgrouping
   */
  class GTSAM_EXPORT Rot3 : public LieGroup<Rot3,3> {

  private:

#ifdef GTSAM_USE_QUATERNIONS
    /** Internal Eigen Quaternion */
    gtsam::Quaternion quaternion_;
#else
    Matrix3 rot_;
#endif

  public:

    /// @name Constructors and named constructors
    /// @{

    /** default constructor, unit rotation */
    Rot3();

    /**
     * Constructor from *columns*
     * @param r1 X-axis of rotated frame
     * @param r2 Y-axis of rotated frame
     * @param r3 Z-axis of rotated frame
     */
    Rot3(const Point3& col1, const Point3& col2, const Point3& col3);

    /** constructor from a rotation matrix, as doubles in *row-major* order !!! */
    Rot3(double R11, double R12, double R13,
        double R21, double R22, double R23,
        double R31, double R32, double R33);

    /**
     * Constructor from a rotation matrix
     * Version for generic matrices. Need casting to Matrix3
     * in quaternion mode, since Eigen's quaternion doesn't
     * allow assignment/construction from a generic matrix.
     * See: http://stackoverflow.com/questions/27094132/cannot-understand-if-this-is-circular-dependency-or-clang#tab-top
     */
    template<typename Derived>
    inline explicit Rot3(const Eigen::MatrixBase<Derived>& R) {
      #ifdef GTSAM_USE_QUATERNIONS
        quaternion_=Matrix3(R);
      #else
        rot_ = R;
      #endif
    }

    /**
     * Constructor from a rotation matrix
     * Overload version for Matrix3 to avoid casting in quaternion mode.
     */
    inline explicit Rot3(const Matrix3& R) {
      #ifdef GTSAM_USE_QUATERNIONS
        quaternion_=R;
      #else
        rot_ = R;
      #endif
    }

    /** Constructor from a quaternion.  This can also be called using a plain
     * Vector, due to implicit conversion from Vector to Quaternion
     * @param q The quaternion
     */
    Rot3(const Quaternion& q);
    Rot3(double w, double x, double y, double z) : Rot3(Quaternion(w, x, y, z)) {}

    /// Random, generates a random axis, then random angle \in [-p,pi]
    static Rot3 Random(boost::mt19937 & rng);

    /** Virtual destructor */
    virtual ~Rot3() {}

    /* Static member function to generate some well known rotations */

    /// Rotation around X axis as in http://en.wikipedia.org/wiki/Rotation_matrix, counterclockwise when looking from unchanging axis.
    static Rot3 Rx(double t);

    /// Rotation around Y axis as in http://en.wikipedia.org/wiki/Rotation_matrix, counterclockwise when looking from unchanging axis.
    static Rot3 Ry(double t);

    /// Rotation around Z axis as in http://en.wikipedia.org/wiki/Rotation_matrix, counterclockwise when looking from unchanging axis.
    static Rot3 Rz(double t);

    /// Rotations around Z, Y, then X axes as in http://en.wikipedia.org/wiki/Rotation_matrix, counterclockwise when looking from unchanging axis.
    static Rot3 RzRyRx(double x, double y, double z);

    /// Rotations around Z, Y, then X axes as in http://en.wikipedia.org/wiki/Rotation_matrix, counterclockwise when looking from unchanging axis.
    inline static Rot3 RzRyRx(const Vector& xyz) {
      assert(xyz.size() == 3);
      return RzRyRx(xyz(0), xyz(1), xyz(2));
    }

    /// Positive yaw is to right (as in aircraft heading). See ypr
    static Rot3 Yaw  (double t) { return Rz(t); }

    /// Positive pitch is up (increasing aircraft altitude).See ypr
    static Rot3 Pitch(double t) { return Ry(t); }

    //// Positive roll is to right (increasing yaw in aircraft).
    static Rot3 Roll (double t) { return Rx(t); }

    /**
     * Returns rotation nRb from body to nav frame.
     * For vehicle coordinate frame X forward, Y right, Z down:
     * Positive yaw is to right (as in aircraft heading).
     * Positive pitch is up (increasing aircraft altitude).
     * Positive roll is to right (increasing yaw in aircraft).
     * Tait-Bryan system from Spatial Reference Model (SRM) (x,y,z) = (roll,pitch,yaw)
     * as described in http://www.sedris.org/wg8home/Documents/WG80462.pdf.
     *
     * For vehicle coordinate frame X forward, Y left, Z up:
     * Positive yaw is to left (as in aircraft heading).
     * Positive pitch is down (decreasing aircraft altitude).
     * Positive roll is to right (decreasing yaw in aircraft).
     */
    static Rot3 Ypr(double y, double p, double r) { return RzRyRx(r,p,y);}

    /** Create from Quaternion coefficients */
    static Rot3 Quaternion(double w, double x, double y, double z) {
      gtsam::Quaternion q(w, x, y, z);
      return Rot3(q);
    }

    /**
     * Convert from axis/angle representation
     * @param  axisw is the rotation axis, unit length
     * @param   angle rotation angle
     * @return incremental rotation
     */
    static Rot3 AxisAngle(const Point3& axis, double angle) {
#ifdef GTSAM_USE_QUATERNIONS
      return gtsam::Quaternion(Eigen::AngleAxis<double>(angle, axis));
#else
      return Rot3(SO3::AxisAngle(axis,angle));
#endif
    }

    /**
     * Convert from axis/angle representation
     * @param   axis is the rotation axis
     * @param   angle rotation angle
     * @return incremental rotation
     */
    static Rot3 AxisAngle(const Unit3& axis, double angle) {
      return AxisAngle(axis.unitVector(),angle);
    }

    /**
     * Rodrigues' formula to compute an incremental rotation
     * @param w a vector of incremental roll,pitch,yaw
     * @return incremental rotation
     */
    static Rot3 Rodrigues(const Vector3& w) {
      return Rot3::Expmap(w);
    }

    /**
     * Rodrigues' formula to compute an incremental rotation
     * @param wx Incremental roll (about X)
     * @param wy Incremental pitch (about Y)
     * @param wz Incremental yaw (about Z)
     * @return incremental rotation
     */
    static Rot3 Rodrigues(double wx, double wy, double wz) {
      return Rodrigues(Vector3(wx, wy, wz));
    }

    /// Determine a rotation to bring two vectors into alignment, using the rotation axis provided
    static Rot3 AlignPair(const Unit3& axis, const Unit3& a_p, const Unit3& b_p);

    /// Calculate rotation from two pairs of homogeneous points using two successive rotations
    static Rot3 AlignTwoPairs(const Unit3& a_p, const Unit3& b_p,  //
                              const Unit3& a_q, const Unit3& b_q);

    /// @}
    /// @name Testable
    /// @{

    /** print */
    void print(const std::string& s="R") const;

    /** equals with an tolerance */
    bool equals(const Rot3& p, double tol = 1e-9) const;

    /// @}
    /// @name Group
    /// @{

    /// identity rotation for group operation
    inline static Rot3 identity() {
      return Rot3();
    }

    /// Syntatic sugar for composing two rotations
    Rot3 operator*(const Rot3& R2) const;

    /// inverse of a rotation, TODO should be different for M/Q
    Rot3 inverse() const {
      return Rot3(Matrix3(transpose()));
    }

    /**
     * Conjugation: given a rotation acting in frame B, compute rotation c1Rc2 acting in a frame C
     * @param cRb rotation from B frame to C frame
     * @return c1Rc2 = cRb * b1Rb2 * cRb'
     */
    Rot3 conjugate(const Rot3& cRb) const {
      // TODO: do more efficiently by using Eigen or quaternion properties
      return cRb * (*this) * cRb.inverse();
    }

    /// @}
    /// @name Manifold
    /// @{

    /**
     * The method retract() is used to map from the tangent space back to the manifold.
     * Its inverse, is localCoordinates(). For Lie groups, an obvious retraction is the
     * exponential map, but this can be expensive to compute. The following Enum is used
     * to indicate which method should be used.  The default
     * is determined by ROT3_DEFAULT_COORDINATES_MODE, which may be set at compile time,
     * and itself defaults to Rot3::CAYLEY, or if GTSAM_USE_QUATERNIONS is defined,
     * to Rot3::EXPMAP.
     */
    enum CoordinatesMode {
      EXPMAP, ///< Use the Lie group exponential map to retract
#ifndef GTSAM_USE_QUATERNIONS
      CAYLEY, ///< Retract and localCoordinates using the Cayley transform.
#endif
      };

#ifndef GTSAM_USE_QUATERNIONS

    // Cayley chart around origin
    struct CayleyChart {
    static Rot3 Retract(const Vector3& v, OptionalJacobian<3, 3> H = boost::none);
    static Vector3 Local(const Rot3& r, OptionalJacobian<3, 3> H = boost::none);
    };

    /// Retraction from R^3 to Rot3 manifold using the Cayley transform
    Rot3 retractCayley(const Vector& omega) const {
      return compose(CayleyChart::Retract(omega));
    }

    /// Inverse of retractCayley
    Vector3 localCayley(const Rot3& other) const {
      return CayleyChart::Local(between(other));
    }

#endif

    /// @}
    /// @name Lie Group
    /// @{

    /**
     * Exponential map at identity - create a rotation from canonical coordinates
     * \f$ [R_x,R_y,R_z] \f$ using Rodrigues' formula
     */
    static Rot3 Expmap(const Vector3& v, OptionalJacobian<3,3> H = boost::none) {
      if(H) *H = Rot3::ExpmapDerivative(v);
#ifdef GTSAM_USE_QUATERNIONS
      return traits<gtsam::Quaternion>::Expmap(v);
#else
      return Rot3(traits<SO3>::Expmap(v));
#endif
    }

    /**
     * Log map at identity - returns the canonical coordinates
     * \f$ [R_x,R_y,R_z] \f$ of this rotation
     */
    static Vector3 Logmap(const Rot3& R, OptionalJacobian<3,3> H = boost::none);

    /// Derivative of Expmap
    static Matrix3 ExpmapDerivative(const Vector3& x);

    /// Derivative of Logmap
    static Matrix3 LogmapDerivative(const Vector3& x);

    /** Calculate Adjoint map */
    Matrix3 AdjointMap() const { return matrix(); }

    // Chart at origin, depends on compile-time flag ROT3_DEFAULT_COORDINATES_MODE
    struct ChartAtOrigin {
      static Rot3 Retract(const Vector3& v, ChartJacobian H = boost::none);
      static Vector3 Local(const Rot3& r, ChartJacobian H = boost::none);
    };

    using LieGroup<Rot3, 3>::inverse; // version with derivative

    /// @}
    /// @name Group Action on Point3
    /// @{

    /**
     * rotate point from rotated coordinate frame to world \f$ p^w = R_c^w p^c \f$
     */
    Point3 rotate(const Point3& p, OptionalJacobian<3,3> H1 = boost::none,
        OptionalJacobian<3,3> H2 = boost::none) const;

    /// rotate point from rotated coordinate frame to world = R*p
    Point3 operator*(const Point3& p) const;

    /// rotate point from world to rotated frame \f$ p^c = (R_c^w)^T p^w \f$
    Point3 unrotate(const Point3& p, OptionalJacobian<3,3> H1 = boost::none,
        OptionalJacobian<3,3> H2=boost::none) const;

    /// @}
    /// @name Group Action on Unit3
    /// @{

    /// rotate 3D direction from rotated coordinate frame to world frame
    Unit3 rotate(const Unit3& p, OptionalJacobian<2,3> HR = boost::none,
        OptionalJacobian<2,2> Hp = boost::none) const;

    /// unrotate 3D direction from world frame to rotated coordinate frame
    Unit3 unrotate(const Unit3& p, OptionalJacobian<2,3> HR = boost::none,
        OptionalJacobian<2,2> Hp = boost::none) const;

    /// rotate 3D direction from rotated coordinate frame to world frame
    Unit3 operator*(const Unit3& p) const;

    /// @}
    /// @name Standard Interface
    /// @{

    /** return 3*3 rotation matrix */
    Matrix3 matrix() const;

    /**
     * Return 3*3 transpose (inverse) rotation matrix
     */
    Matrix3 transpose() const;
    // TODO: const Eigen::Transpose<const Matrix3> transpose() const;

    /// @deprecated, this is base 1, and was just confusing
    Point3 column(int index) const;

    Point3 r1() const; ///< first column
    Point3 r2() const; ///< second column
    Point3 r3() const; ///< third column

    /**
     * Use RQ to calculate xyz angle representation
     * @return a vector containing x,y,z s.t. R = Rot3::RzRyRx(x,y,z)
     */
    Vector3 xyz() const;

    /**
     * Use RQ to calculate yaw-pitch-roll angle representation
     * @return a vector containing ypr s.t. R = Rot3::Ypr(y,p,r)
     */
    Vector3 ypr() const;

    /**
     * Use RQ to calculate roll-pitch-yaw angle representation
     * @return a vector containing ypr s.t. R = Rot3::Ypr(y,p,r)
     */
    Vector3 rpy() const;

    /**
     * Accessor to get to component of angle representations
     * NOTE: these are not efficient to get to multiple separate parts,
     * you should instead use xyz() or ypr()
     * TODO: make this more efficient
     */
    inline double roll() const  { return ypr()(2); }

    /**
     * Accessor to get to component of angle representations
     * NOTE: these are not efficient to get to multiple separate parts,
     * you should instead use xyz() or ypr()
     * TODO: make this more efficient
     */
    inline double pitch() const { return ypr()(1); }

    /**
     * Accessor to get to component of angle representations
     * NOTE: these are not efficient to get to multiple separate parts,
     * you should instead use xyz() or ypr()
     * TODO: make this more efficient
     */
    inline double yaw() const   { return ypr()(0); }

    /// @}
    /// @name Advanced Interface
    /// @{

    /** Compute the quaternion representation of this rotation.
     * @return The quaternion
     */
    gtsam::Quaternion toQuaternion() const;

    /**
     * Converts to a generic matrix to allow for use with matlab
     * In format: w x y z
     */
    Vector quaternion() const;

    /**
     * @brief Spherical Linear intERPolation between *this and other
     * @param s a value between 0 and 1
     * @param other final point of iterpolation geodesic on manifold
     */
    Rot3 slerp(double t, const Rot3& other) const;

    /// Output stream operator
    GTSAM_EXPORT friend std::ostream &operator<<(std::ostream &os, const Rot3& p);

    /// @}

#ifdef GTSAM_ALLOW_DEPRECATED_SINCE_V4
    /// @name Deprecated
    /// @{
    static Rot3 rodriguez(const Point3&  axis, double angle) { return AxisAngle(axis, angle); }
    static Rot3 rodriguez(const Unit3&   axis, double angle) { return AxisAngle(axis, angle); }
    static Rot3 rodriguez(const Vector3& w)                  { return Rodrigues(w); }
    static Rot3 rodriguez(double wx, double wy, double wz)   { return Rodrigues(wx, wy, wz); }
    static Rot3 yaw  (double t) { return Yaw(t); }
    static Rot3 pitch(double t) { return Pitch(t); }
    static Rot3 roll (double t) { return Roll(t); }
    static Rot3 ypr(double y, double p, double r) { return Ypr(r,p,y);}
    static Rot3 quaternion(double w, double x, double y, double z) {
      return Rot3::Quaternion(w, x, y, z);
    }
  /// @}
#endif

  private:

    /** Serialization function */
    friend class boost::serialization::access;
    template<class ARCHIVE>
    void serialize(ARCHIVE & ar, const unsigned int /*version*/)
    {
#ifndef GTSAM_USE_QUATERNIONS
       ar & boost::serialization::make_nvp("rot11", rot_(0,0));
       ar & boost::serialization::make_nvp("rot12", rot_(0,1));
       ar & boost::serialization::make_nvp("rot13", rot_(0,2));
       ar & boost::serialization::make_nvp("rot21", rot_(1,0));
       ar & boost::serialization::make_nvp("rot22", rot_(1,1));
       ar & boost::serialization::make_nvp("rot23", rot_(1,2));
       ar & boost::serialization::make_nvp("rot31", rot_(2,0));
       ar & boost::serialization::make_nvp("rot32", rot_(2,1));
       ar & boost::serialization::make_nvp("rot33", rot_(2,2));
#else
      ar & boost::serialization::make_nvp("w", quaternion_.w());
      ar & boost::serialization::make_nvp("x", quaternion_.x());
      ar & boost::serialization::make_nvp("y", quaternion_.y());
      ar & boost::serialization::make_nvp("z", quaternion_.z());
#endif
    }

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  };

  /**
   * [RQ] receives a 3 by 3 matrix and returns an upper triangular matrix R
   * and 3 rotation angles corresponding to the rotation matrix Q=Qz'*Qy'*Qx'
   * such that A = R*Q = R*Qz'*Qy'*Qx'. When A is a rotation matrix, R will
   * be the identity and Q is a yaw-pitch-roll decomposition of A.
   * The implementation uses Givens rotations and is based on Hartley-Zisserman.
   * @param A 3 by 3 matrix A=RQ
   * @return an upper triangular matrix R
   * @return a vector [thetax, thetay, thetaz] in radians.
   */
  GTSAM_EXPORT std::pair<Matrix3,Vector3> RQ(const Matrix3& A);

  template<>
  struct traits<Rot3> : public internal::LieGroup<Rot3> {};

  template<>
  struct traits<const Rot3> : public internal::LieGroup<Rot3> {};
}


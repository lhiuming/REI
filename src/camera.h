#ifndef REI_CAMERA_H
#define REI_CAMERA_H

#include "algebra.h"

/*
 * camera.h
 * Define the Camera class, used to created a viewport and provide world-to-
 * camera transformation. Return column-major matrox to transform row-vetors;
 * and convert Right-Handed world space coordinates to Left-Hand coordinates
 * (in camera space or normalize device space / clip space).
 *
 * TODO: add sematic parameters (focus distance, like a real camera)
 * TODO: add depth
 * TODO: support tiltering ?
 */

namespace rei {

class Camera {
public:
  // Default constructor (at origin, looking at -z axis)
  Camera();

  // Initialize with position and direction
  Camera(const Vec3& pos, const Vec3& dir = {0.0, 0.0, -1.0}, const Vec3& up = {0.0, 1.0, 0.0});

  // Configurations
  void set_aspect(double width2height); // widht / height
  void set_params(double aspect, double angle, double znear, double zfar);

  // May-be useful queries
  double aspect() const { return m_aspect; }
  double fov_h() const { return angle; }
  double fov_v() const { return angle / m_aspect; }
  Vec3 position() const { return m_position; }
  Vec3 right() const { return cross(m_direction, m_up); }

  // Dynamic Configurations
  void zoom(double quantity);
  void move(double right, double up, double fwd);
  void rotate_position(const Vec3& center, const Vec3& axis, double radian);
  void rotate_direction(const Vec3& axis, double radian);
  void look_at(const Vec3& target, const Vec3& up_hint = {0, 0, 0});

  // Get transforms (result in Left Hand Coordinate;
  // used to transform row-vector)
  const Mat4& get_w2c() const { return world2camera; }
  const Mat4& get_c2n() const { return camera2normalized; }
  const Mat4& get_w2n() const { return world2normalized; }
  const Mat4& get_w2v() const { return world2viewport; }

  // Visibility query
  bool visible(const Vec3& v) const;

  // Debug print
  friend std::wostream& operator<<(std::wostream& os, const Camera& cam);

private:
  // Note: position and direction are in right-haneded world space
  Vec3 m_up = Vec3(0.0, 1.0, 0.0);
  Vec3 m_position = Vec3(0.0, 0.0, 0.0);
  Vec3 m_direction = Vec3(0.0, 0.0, -1.0); // looking at -z axis in world
  double angle = 60;                     // horizontal view-angle range, by degree
  double m_aspect = 4.0 / 3.0;             // width / height
  double znear = 1.0, zfar = 1000.0;     // distance of two planes of the frustrum

  Mat4 world2camera;      // defined by position and direction
  Mat4 camera2normalized; // defined by view angle and ration
  Mat4 world2normalized;  // composed from above 2
  Mat4 world2viewport;    // added a static normalized->viewport step

  // helpers to update transforms
  void update_w2c();
  void update_c2n();
  void update_w2n();
  void update_transforms() {
    update_w2c();
    update_c2n();
    update_w2n();
  }
  void mark_view_trans_dirty() {
    // normalize the up direction
    m_up = m_up - dot(m_up, m_direction) * m_direction;
    Vec3::normalize(m_up);
    update_transforms();
  }
  void mark_proj_trans_dirty() { update_transforms(); }
};

} // namespace rei

#endif

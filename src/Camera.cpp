#include "Camera.h"

glm::vec2 Camera::focal(glm::uvec2 img_size) const
{
    return glm::vec2(
        static_cast<float>(fov_to_focal(m_fov_x, img_size.x)),
        static_cast<float>(fov_to_focal(m_fov_x, img_size.y))
    );
}

glm::vec2 Camera::center(glm::uvec2 img_size) const
{
    return glm::vec2(
        m_center_uv.x * static_cast<float>(img_size.x),
        m_center_uv.y * static_cast<float>(img_size.y)
    );
}

glm::mat4 Camera::local_to_world() const
{
    return glm::translate(glm::mat4(1.f), m_position) * glm::mat4_cast(m_rotation);
}

glm::mat4 Camera::world_to_local() const
{
    return glm::inverse(local_to_world());
}

double Camera::fov_to_focal(double fov_rad, uint32_t pixels)
{
    return .5 * static_cast<double>(pixels) / tan(.5 * fov_rad);
}

double Camera::focal_to_fov(double focal, uint32_t pixels)
{
    return 2. * atan(static_cast<double>(pixels) / (2. * focal));
}

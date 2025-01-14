#ifndef MATH_H
#define MATH_H

#define PI 3.14159265358979323846264338327950288f
#define SQUARE(x) ((x)*(x))
#define TO_RAD(x) ((x) * PI / 180.0f)
#define TO_DEG(x) ((x) * 180.0f / PI)
#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#define MIN(x,y) ((x) < (y) ? (y) : (x))

// @todo:
// - make everything simd
#define USE_SSE 1

r32 clampf(r32 x, r32 bottom, r32 top)
{
    if (x < bottom)
    {
        x = bottom;
    }
    else if (x > top)
    {
        x = top;
    }

    return x;
}


// ==== Vector Math ====
union Vec2 {
  struct {
    r32 x;
    r32 y;
  };
  r32 data[2];

  Vec2 operator+(r32 scaler) {
    Vec2 res;
    res.x = this->x + scaler;
    res.y = this->y + scaler;

    return res;
  }

  Vec2 operator+(Vec2 v) {
    Vec2 res;
    res.x = this->x + v.x;
    res.y = this->y + v.y;

    return res;
  }

  Vec2 operator-(r32 scaler) {
    Vec2 res;
    res.x = this->x - scaler;
    res.y = this->y - scaler;

    return res;
  }

  Vec2 operator-(Vec2 v) {
    Vec2 res;
    res.x = this->x - v.x;
    res.y = this->y - v.y;

    return res;
  }

  Vec2 operator*(r32 scaler) {
    Vec2 res;
    res.x = this->x * scaler;
    res.y = this->y * scaler;

    return res;
  }

  Vec2 operator*(Vec2 v) {
    Vec2 res;
    res.x = this->x * v.x;
    res.y = this->y * v.y;

    return res;
  }

  Vec2 operator/(r32 scaler) {
    SDL_assert(scaler != 0);
    Vec2 res;
    res.x = this->x / scaler;
    res.y = this->y / scaler;

    return res;
  }
};

union Vec3 {
    struct {
	r32 x;
	r32 y;
	r32 z;
    };
    r32 data[3];

  Vec2 v2() {
    return Vec2{x, y};
  }
};

typedef Vec3 RGB;

union Vec4 {
    struct {
	r32 x;
	r32 y;
	r32 z;
	r32 w;
    };
    r32 data[4];
    __m128 sse;
};

// @note: matrix and all matrix operations will be done in column major
union Mat4 {
    Vec4 row[4];
    r32 data[4][4];
    r32 buffer[16];
};

// ==== Vec2 ====
Vec2 vec2(r32 s) {
  return Vec2{s, s};
}

// @note: written for completeness sake
Vec2 add2vf(Vec2 v, r32 s);
Vec2 add2v(Vec2 a, Vec2 b);
Vec2 subtract2vf(Vec2 v, r32 s);
Vec2 subtract2v(Vec2 a, Vec2 b);
Vec2 multiply2v(Vec2 a, Vec2 b);

r32 dot2v(Vec2 a, Vec2 b) {
  r32 res = (a.x*b.x)+(a.y*b.y);
  return res;
}

Vec2 multiply2vf(Vec2 vec, r32 scaler) {
  Vec2 res;
  res.x = vec.x * scaler;
  res.y = vec.y * scaler;

  return res;
}

Vec2 divide2vf(Vec2 vec, r32 scaler) {
  SDL_assert(scaler != 0);
  Vec2 res;
  res.x = vec.x / scaler;
  res.y = vec.y / scaler;

  return res;
}

Vec2 divide2v(Vec2 a, Vec2 b) {
  SDL_assert(b.x != 0 && b.y != 0);
  Vec2 res;
  res.x = a.x / b.x;
  res.y = a.y / b.y;

  return res;
}

r32 magnitude2v(Vec2 v) {
    r32 res = sqrtf(SQUARE(v.x) + SQUARE(v.y));
    return res;
}

Vec2 normalize2v(Vec2 v) {
	r32 magnitude = magnitude2v(v);
	Vec2 res = divide2vf(v, magnitude);
	return res;
}

// ========================================================== Vec3 ==========================================================

// @note: Written for completeness sake
Vec3 vec3(r32 s);
Vec3 subtract3vf(Vec3 v, r32 scaler);
Vec3 multiply3v(Vec3 a, Vec3 b);
Vec3 divide3v(Vec3 a, Vec3 b);

Vec3 add3vf(Vec3 vec, r32 scaler)
{
	Vec3 res;
	res.x = vec.x + scaler;
	res.y = vec.y + scaler;
	res.z = vec.z + scaler;

	return res;
}

Vec3 add3v(Vec3 a, Vec3 b)
{
  Vec3 res;
  res.x = a.x + b.x;
  res.y = a.y + b.y;
  res.z = a.z + b.z;

  return res;
}

Vec3 subtract3v(Vec3 a, Vec3 b)
{
	Vec3 res;
	res.x = a.x - b.x;
	res.y = a.y - b.y;
	res.z = a.z - b.z;

	return res;
}

Vec3 multiply3vf(Vec3 vec, r32 scaler)
{
	Vec3 res;
	res.x = vec.x * scaler;
	res.y = vec.y * scaler;
	res.z = vec.z * scaler;

	return res;
}


Vec3 divide3vf(Vec3 vec, r32 scaler)
{
	Vec3 res;
	res.x = vec.x / scaler;
	res.y = vec.y / scaler;
	res.z = vec.z / scaler;

	return res;
}

r32 dot3v(Vec3 a, Vec3 b)
{
	r32 x = a.x * b.x;
	r32 y = a.y * b.y;
	r32 z = a.z * b.z;

	r32 res = x + y + z;

	return res;
}

r32 magnitude3v(Vec3 vec)
{
	r32 res = sqrtf(SQUARE(vec.x) + SQUARE(vec.y) + SQUARE(vec.z));
	return res;
}

Vec3 normalize3v(Vec3 vec)
{
	r32 magnitude = magnitude3v(vec);
	Vec3 res = divide3vf(vec, magnitude);
	return res;
}

Vec3 cross3v(Vec3 a, Vec3 b)
{
	Vec3 res;
	res.x = (a.y * b.z) - (a.z * b.y);
	res.y = (a.z * b.x) - (a.x * b.z);
	res.z = (a.x * b.y) - (a.y * b.x);

	return res;
}

// ============================================== Vec4, Mat4 ==============================================
static u64 tick_freq = SDL_GetPerformanceFrequency();
static u64 cum_math_ticks = 0;
static r64 cum_math_time = 0.0f;
// ==================== Vec4 ====================
Vec4 vec4(r32 s)
{
	Vec4 res;
#if USE_SSE
	res.sse = _mm_set_ps1(s);
#else
	res.x = s;
	res.y = s;
	res.z = s;
	res.w = s;
#endif

	return res;
}

Vec4 vec4(r32 x, r32 y, r32 z, r32 w)
{
	Vec4 res;
#if USE_SSE
	res.sse = _mm_setr_ps(x, y, z, w);
#else
	res.x = x;
	res.y = y;
	res.z = z;
	res.w = w;
#endif

	return res;
}

// @note: Written for completeness sake.
Vec4 add4vf(Vec4 vec, r32 scaler);
Vec4 add4v(Vec4 a, Vec4 b);
Vec4 subtract4vf(Vec4 vec, r32 scaler);
Vec4 subtract4v(Vec4 a, Vec4 b);
Vec4 multiply4vf(Vec4 vec, r32 scaler);
Vec4 multiply4v(Vec4 a, Vec4 b);
Vec4 divide4vf(Vec4 vec, r32 scaler);
Vec4 divide4v(Vec4 a, Vec4 b);
Vec4 dot4v(Vec4 a, Vec4 b);

// =================== MAT4 ===================
Mat4 mat4(r32 s) {
  Mat4 res;
  memset(&res, s, sizeof(res));

  return res;
}

Mat4 diag4m(r32 value) {
	Mat4 res = mat4(0.0f);
	res.data[0][0] = value;
	res.data[1][1] = value;
	res.data[2][2] = value;
	res.data[3][3] = value;

	return res;
}

Mat4 add4m(Mat4 a, Mat4 b)
{
	Mat4 res;
	// row 0
	res.data[0][0] = a.data[0][0] + b.data[0][0];
	res.data[0][1] = a.data[0][1] + b.data[0][1];
	res.data[0][2] = a.data[0][2] + b.data[0][2];
	res.data[0][3] = a.data[0][3] + b.data[0][3];
	// row 1
	res.data[1][0] = a.data[1][0] + b.data[1][0];
	res.data[1][1] = a.data[1][1] + b.data[1][1];
	res.data[1][2] = a.data[1][2] + b.data[1][2];
	res.data[1][3] = a.data[1][3] + b.data[1][3];
	// row 2
	res.data[2][0] = a.data[2][0] + b.data[2][0];
	res.data[2][1] = a.data[2][1] + b.data[2][1];
	res.data[2][2] = a.data[2][2] + b.data[2][2];
	res.data[2][3] = a.data[2][3] + b.data[2][3];
	// row 3
	res.data[3][0] = a.data[3][0] + b.data[3][0];
	res.data[3][1] = a.data[3][1] + b.data[3][1];
	res.data[3][2] = a.data[3][2] + b.data[3][2];
	res.data[3][3] = a.data[3][3] + b.data[3][3];

	return res;
}

Mat4 subtract4m(Mat4 a, Mat4 b)
{
	Mat4 res;
	// row 0
	res.data[0][0] = a.data[0][0] - b.data[0][0];
	res.data[0][1] = a.data[0][1] - b.data[0][1];
	res.data[0][2] = a.data[0][2] - b.data[0][2];
	res.data[0][3] = a.data[0][3] - b.data[0][3];
	// row 1
	res.data[1][0] = a.data[1][0] - b.data[1][0];
	res.data[1][1] = a.data[1][1] - b.data[1][1];
	res.data[1][2] = a.data[1][2] - b.data[1][2];
	res.data[1][3] = a.data[1][3] - b.data[1][3];
	// row 2
	res.data[2][0] = a.data[2][0] - b.data[2][0];
	res.data[2][1] = a.data[2][1] - b.data[2][1];
	res.data[2][2] = a.data[2][2] - b.data[2][2];
	res.data[2][3] = a.data[2][3] - b.data[2][3];
	// row 3
	res.data[3][0] = a.data[3][0] - b.data[3][0];
	res.data[3][1] = a.data[3][1] - b.data[3][1];
	res.data[3][2] = a.data[3][2] - b.data[3][2];
	res.data[3][3] = a.data[3][3] - b.data[3][3];

	return res;
}

Vec4 multiply4mv(Mat4 m, Vec4 v)
{
    r64 prev_tick = SDL_GetPerformanceCounter();

    Vec4 res = vec4(0);
#if USE_SSE
    __m128 scalar = _mm_shuffle_ps(v.sse, v.sse, 0x0);
    res.sse = _mm_mul_ps(scalar, m.row[0].sse);

    scalar = _mm_shuffle_ps(v.sse, v.sse, 0x55);
    __m128 mult = _mm_mul_ps(scalar, m.row[1].sse);
    res.sse = _mm_add_ps(res.sse, mult);

    scalar = _mm_shuffle_ps(v.sse, v.sse, 0xaa);
    mult = _mm_mul_ps(scalar, m.row[2].sse);
    res.sse = _mm_add_ps(res.sse, mult);

    scalar = _mm_shuffle_ps(v.sse, v.sse, 0xff);
    mult = _mm_mul_ps(scalar, m.row[3].sse);
    res.sse = _mm_add_ps(res.sse, mult);
#else

  res.x += v.x*m.data[0][0];
  res.y += v.x*m.data[0][1];
  res.z += v.x*m.data[0][2];
  res.w += v.x*m.data[0][3];

  res.x += v.y*m.data[1][0];
  res.y += v.y*m.data[1][1];
  res.z += v.y*m.data[1][2];
  res.w += v.y*m.data[1][3];

  res.x += v.z*m.data[2][0];
  res.y += v.z*m.data[2][1];
  res.z += v.z*m.data[2][2];
  res.w += v.z*m.data[2][3];

  res.x += v.w*m.data[3][0];
  res.y += v.w*m.data[3][1];
  res.z += v.w*m.data[3][2];
  res.w += v.w*m.data[3][3];
#endif

    r64 curr_tick = SDL_GetPerformanceCounter();
    cum_math_ticks += curr_tick - prev_tick;

  return res;
}

Mat4 multiply4m(Mat4 a, Mat4 b)
{
  Mat4 res = { 0 };

  res.row[0] = multiply4mv(a, b.row[0]);
  res.row[1] = multiply4mv(a, b.row[1]);
  res.row[2] = multiply4mv(a, b.row[2]);
  res.row[3] = multiply4mv(a, b.row[3]);

  return res;
}
// ==== Matrix Transformation ====

Mat4 scaling_matrix4m(r32 x, r32 y, r32 z)
{
  // generates a 4x4 scaling matrix for scaling each of the x,y,z axis
  Mat4 res = diag4m(1.0f);
  res.data[0][0] = x;
  res.data[1][1] = y;
  res.data[2][2] = z;

  return res;
}

Mat4 translation_matrix4m(r32 x, r32 y, r32 z)
{
  Mat4 res = diag4m(1.0f);
  res.row[3] = Vec4{x, y, z, 1.0f};

  return res;
}

Mat4 rotation_matrix4m(r32 angle_radians, Vec3 axis)
{
  Mat4 res = diag4m(1.0f);
  axis = normalize3v(axis);

  r32 cos_theta = cosf(angle_radians);
  r32 sin_theta = sinf(angle_radians);
  r32 cos_value = 1.0f - cos_theta;

  res.data[0][0] = (axis.x * axis.x * cos_value) + cos_theta;
  res.data[0][1] = (axis.x * axis.y * cos_value) + (axis.z * sin_theta);
  res.data[0][2] = (axis.x * axis.z * cos_value) - (axis.y * sin_theta);

  res.data[1][0] = (axis.x * axis.y * cos_value) - (axis.z * sin_theta);
  res.data[1][1] = (axis.y * axis.y * cos_value) + cos_theta;
  res.data[1][2] = (axis.y * axis.z * cos_value) + (axis.x * sin_theta);

  res.data[2][0] = (axis.x * axis.z * cos_value) + (axis.y * sin_theta);
  res.data[2][1] = (axis.y * axis.z * cos_value) - (axis.x * sin_theta);
  res.data[2][2] = (axis.z * axis.z * cos_value) + cos_value;

  return res;
}

Mat4 orthographic4m(r32 left, r32 right, r32 bottom, r32 top, r32 near, r32 far)
{
  Mat4 res = diag4m(0);

  res.data[0][0] = 2.0f/(right - left);
  res.data[1][1] = 2.0f/(top - bottom);
  res.data[2][2] = 2.0f/(near - far);
  res.data[3][0] = (right + left)/(left - right);
  res.data[3][1] = (top + bottom)/(bottom - top);
  res.data[3][2] = (far + near)/(near - far);
  res.data[3][3] = 1.0f;

  return res;
}

Mat4 lookat4m(Vec3 up, Vec3 forward, Vec3 right, Vec3 position)
{
	/*
	* @note: The construction of the lookat matrix is not obvious. For that reason here is the supplemental matrial I have used to understand
	* things while I maintain my elementary understanding of linear algebra.
	* 1. This youtube video (https://www.youtube.com/watch?v=3ZmqJb7J5wE) helped me understand why we invert matrices.
	*		 It is because, we are moving from the position matrix which is a global to the view matrix which
	*		 is a local. It won't be very clear from this illustration alone, so you would be best served watching the video and recollecting and understanding from there.
	* 2. This article (https://twodee.org/blog/17560) derives (or rather shows), in a very shallow way how we get to the look at matrix.
	*/
	Mat4 res = diag4m(1.0f);
	res.row[0] = Vec4{ right.x,   up.x,   forward.x,		0.0f };
	res.row[1] = Vec4{ right.y,   up.y,   forward.y,	  0.0f };
	res.row[2] = Vec4{ right.z,   up.z,   forward.z,    0.0f };

	res.data[3][0] = -dot3v(right, position);
	res.data[3][1] = -dot3v(up, position);
	res.data[3][2] = -dot3v(forward, position);
  res.data[3][3] = 1.0f;

	return res;
}

Vec3 camera_look_around(r32 angle_pitch, r32 angle_yaw)
{
  Vec3 camera_look = {0.0};
  camera_look.x = cosf(angle_yaw) * cosf(angle_pitch);
  camera_look.y = sinf(angle_pitch);
  camera_look.z = sinf(angle_yaw) * cosf(angle_pitch);
  camera_look = normalize3v(camera_look);

  return camera_look;
}

Mat4 camera_create4m(Vec3 camera_pos, Vec3 camera_look, Vec3 camera_up)
{
	// @note: We do this because this allows the camera to have the axis it looks at
	// inwards be the +z axis.
	// If we did not do this, then the inward axis the camera looks at would be negative.
	// I am still learning from learnopengl.com but I imagine that this was done for conveniences' sake.
	Vec3 camera_forward_dir = normalize3v(subtract3v(camera_pos, camera_look));
	Vec3 camera_right_dir   = normalize3v(cross3v(camera_up, camera_forward_dir));
	Vec3 camera_up_dir      = normalize3v(cross3v(camera_forward_dir, camera_right_dir));

	Mat4 res = lookat4m(camera_up_dir, camera_forward_dir, camera_right_dir, camera_pos);

	return res;
}

#endif

#ifndef MATH_H
#define MATH_H

#define PI 3.14159265358979323846264338327950288f
#define SQUARE(x) ((x)*(x))
#define TO_RAD(x) ((x) * PI / 180.0f)
#define TO_DEG(x) ((x) * 180.0f / PI)
#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#define MIN(x,y) ((x) < (y) ? (y) : (x))

// @todo: 
// - convert these to column major calculations for the opengl path
// - make everything simd

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
};

union Mat4 {
	Vec4 xyzw[4];
	r32 data[4][4];
	r32 buffer[16];
};

// ==== Vec2 ====
Vec2 v2(r32 v) {
  return Vec2{v, v};
}

Vec2 v2(r32 x, r32 y) {
  return Vec2{x, y};
}

r32 dot2v(Vec2 a, Vec2 b) 
{
  r32 res = (a.x*b.x)+(a.y*b.y);
  return res;
}

Vec2 mul2vf(Vec2 vec, r32 scaler)
{
  Vec2 res;
  res.x = vec.x * scaler;
  res.y = vec.y * scaler;

  return res;
}

Vec2 div2vf(Vec2 vec, r32 scaler)
{
  SDL_assert(scaler != 0);
  Vec2 res;
  res.x = vec.x / scaler;
  res.y = vec.y / scaler;

  return res;
}

r32 magnitude2v(Vec2 v)
{
	r32 res = sqrtf(SQUARE(v.x) + SQUARE(v.y));
	return res;
}

Vec2 normalize2v(Vec2 v)
{
	r32 magnitude = magnitude2v(v);
	Vec2 res = div2vf(v, magnitude);
	return res;
}

// ========================================================== Vec3 ==========================================================

Vec3 init3v(r32 x, r32 y, r32 z)
{
	Vec3 res;
	res.x = x;
	res.y = y;
	res.z = z;
    
	return res;
}

Vec3 scaler_add3v(Vec3 vec, r32 scaler)
{
	Vec3 res;
	res.x = vec.x + scaler;
	res.y = vec.y + scaler;
	res.z = vec.z + scaler;
    
	return res;
}

Vec3 scaler_multiply3v(Vec3 vec, r32 scaler)
{
	Vec3 res;
	res.x = vec.x * scaler;
	res.y = vec.y * scaler;
	res.z = vec.z * scaler;
    
	return res;
}

Vec3 scaler_divide3v(Vec3 vec, r32 scaler)
{
	Vec3 res;
	res.x = vec.x / scaler;
	res.y = vec.y / scaler;
	res.z = vec.z / scaler;
    
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

r32 dot_multiply3v(Vec3 a, Vec3 b)
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
	Vec3 res = scaler_divide3v(vec, magnitude);
	return res;
}

Vec3 cross_multiply3v(Vec3 a, Vec3 b)
{
	Vec3 res;
	res.x = (a.y * b.z) - (a.z * b.y);
	res.y = (a.z * b.x) - (a.x * b.z);
	res.z = (a.x * b.y) - (a.y * b.x);
    
	return res;
}

// ============================================== Vec4, Mat4 ==============================================

Vec4 init4v(r32 x, r32 y, r32 z, r32 w)
{
	Vec4 res;
	res.x = x;
	res.y = y;
	res.z = z;
	res.w = w;
    
	return res;
}

Mat4 init_value4m(r32 value)
{
	Mat4 res = {0};
	res.data[0][0] = value;
	res.data[1][1] = value;
	res.data[2][2] = value;
	res.data[3][3] = value;
    
	return res;
}

// @note: These operations are just defined and not expressed. They are kept here for completeness sake BUT
// since I have not had to do anything related to these, I have not created them.
Vec4 scaler_add4v(Vec4 vec, r32 scaler);
Vec4 scaler_subtract4v(Vec4 vec, r32 scaler);
Vec4 scaler_multiply4v(Vec4 vec, r32 scaler);
Vec4 scaler_divide4v(Vec4 vec, r32 scaler);
Vec4 add4v(Vec4 a, Vec4 b);
Vec4 subtract4v(Vec4 a, Vec4 b);
Vec4 dot_multiply4v(Vec4 a, Vec4 b);

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

Vec4 multiply4vm(Vec4 vec, Mat4 mat)
{
    /*
     * @note: Incase I get confused about this in the future.
     *
     * Everything is row-order, which means that things in memory are laid out row first. So with a sample matrix
     * we have this order in memory: r1c1 r1c2 r1c3 r1c4 r2c1 ... (r = row, c = column). The same holds true for 
     * vectors. (maybe move this explanation to the top)
     *
     * Now, multiply4vm will multiply a vector with a matrix. Conventionally that does not make any sense as
     * a vector is usually 4x1 and a matrix ix 4x4.
     * What this function considers a vector, while it is a vector, it is infact a row from a matrix, which
     * means that the vector is 1x4 and the matrix is 4x4.
     * 
     * The function is meant to supplement the matrix multiplication process to alleviate the multiple lines of code
     * we have to write when multiplying the row of a left matrix to each column of the right matrix
     */
	Vec4 res = { 0 };
	res.x = (mat.data[0][0] * vec.x) + (mat.data[0][1] * vec.y) + (mat.data[0][2] * vec.z) + (mat.data[0][3] * vec.w);
	res.y = (mat.data[1][0] * vec.x) + (mat.data[1][1] * vec.y) + (mat.data[1][2] * vec.z) + (mat.data[1][3] * vec.w);
	res.z = (mat.data[2][0] * vec.x) + (mat.data[2][1] * vec.y) + (mat.data[2][2] * vec.z) + (mat.data[2][3] * vec.w);
	res.w = (mat.data[3][0] * vec.x) + (mat.data[3][1] * vec.y) + (mat.data[3][2] * vec.z) + (mat.data[3][3] * vec.w);
	
	return res;
}

Mat4 multiply4m(Mat4 a, Mat4 b)
{
	Mat4 res = { 0 };
	
	res.xyzw[0] = multiply4vm(a.xyzw[0], b);
	res.xyzw[1] = multiply4vm(a.xyzw[1], b);
	res.xyzw[2] = multiply4vm(a.xyzw[2], b);
	res.xyzw[3] = multiply4vm(a.xyzw[3], b);
    
	return res;
}

// ==== Matrix Transformation ====

Mat4 scaling_matrix4m(r32 x, r32 y, r32 z)	// generates a 4x4 scaling matrix for scaling each of the x,y,z axis
{
	Mat4 res = init_value4m(1.0f);
	res.data[0][0] = x;
	res.data[1][1] = y;
	res.data[2][2] = z;
    
	return res;
}

Mat4 translation_matrix4m(r32 x, r32 y, r32 z)	// generates a 4x4 translation matrix for translation along each of the x,y,z axis
{
	Mat4 res = init_value4m(1.0f);
	res.data[0][3] = x;
	res.data[1][3] = y;
	res.data[2][3] = z;
    
	return res;
}

Mat4 rotation_matrix4m(r32 angle_radians, Vec3 axis)	// generates a 4x4 rotation matrix for rotation along each of the x,y,z axis
{
	Mat4 res = init_value4m(1.0f);
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
	res.data[2][1] = (axis.z * axis.y * cos_value) - (axis.x * sin_theta);
	res.data[2][2] = (axis.z * axis.z * cos_value) + cos_theta;
    
	return res;
}

Mat4 perspective_projection4m(r32 left, r32 right, r32 bottom, r32 top, r32 near, r32 far)
{
	Mat4 res = { 0 };
	
	res.data[0][0] = (2.0 * near)/(right - left);
	res.data[0][2] = (right + left)/(right - left);
    
	res.data[1][1] = (2.0 * near)/(top - bottom);
	res.data[1][2] = (top + bottom)/(top - bottom);
    
	res.data[2][2] = -(far + near)/(far - near);
	res.data[2][3] = -2.0*far*near/(far - near);
    
	res.data[3][2] = -1.0;
    
	return res;
}

Mat4 perspective4m(r32 fov, r32 aspect_ratio, r32 near, r32 far)
{
	r32 cotangent = 1.0f / tanf(fov / 2.0f);
	
	Mat4 res = { 0 };
    
	res.data[0][0] = cotangent / aspect_ratio;
	
	res.data[1][1] = cotangent;
    
	res.data[2][2] = -(far + near) / (far - near);
	res.data[2][3] = -2.0f * far * near / (far - near);
    
	res.data[3][2] = -1.0f;
    
	return res;
}

Mat4 orthographic_projection4m(r32 left, r32 right, r32 bottom, r32 top, r32 near, r32 far)
{
  // @todo: understand the derivation once I am done experimenting
  Mat4 res = { 0 };
  res.data[0][0] = 2.0f/(right - left);   res.data[0][3] = -(right + left)/(right - left);
  res.data[1][1] = 2.0f/(top - bottom);   res.data[1][3] = -(top + bottom)/(top - bottom);
  res.data[2][2] = -2.0f/(far - near);    res.data[2][3] = -(far + near)/(far - near);
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
  *
  * I am guessing this is not useful for 2D stuff
	*/
	Mat4 res = init_value4m(1.0);
	res.xyzw[0] = Vec4{ right.x,		right.y,	 right.z,		-dot_multiply3v(right, position) };
	res.xyzw[1] = Vec4{ up.x,				up.y,			 up.z,			-dot_multiply3v(up, position) };
	res.xyzw[2] = Vec4{ forward.x,  forward.y, forward.z, -dot_multiply3v(forward, position) };
	res.xyzw[3] = Vec4{ 0.0f,				0.0f,			 0.0f,			 1.0f };
    
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
	Vec3 camera_right_dir   = normalize3v(cross_multiply3v(camera_up, camera_forward_dir));
	Vec3 camera_up_dir      = normalize3v(cross_multiply3v(camera_forward_dir, camera_right_dir));
    
	Mat4 res = lookat4m(camera_up_dir, camera_forward_dir, camera_right_dir, camera_pos);
    
	return res;
}
#endif

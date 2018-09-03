#include <vector>
#include <cstdlib>
#include <cmath>

class approx_pixel {
	public:
		approx_pixel();
		float r; 
		float g; 
		float b;
};

class image {
	public:
		approx_pixel* approx_pixels[100][100];
};

int main() {
	
	float w[3];

	image* approx_image_ptr;
	for (int i = 0; i < 100; ++i) {
		float a = 1.0f;
		a *= a;
		approx_image_ptr->approx_pixels[i][1]->r *= a;
		//approx_image_ptr->approx_pixels[i][1]->g *= 2.0f;
		//approx_image_ptr->approx_pixels[i][1]->b *= 3.0f;
	}

	approx_pixel* tmp = approx_image_ptr->approx_pixels[2][1];
	float result = log(tmp->r + tmp->g + tmp->b);
	approx_image_ptr->approx_pixels[2][1]->r = result;

	w[0] = approx_image_ptr->approx_pixels[2][1]->r;
	w[1] = approx_image_ptr->approx_pixels[2][1]->g;
	w[2] = approx_image_ptr->approx_pixels[2][1]->b;

	w[1] *= w[2];
}

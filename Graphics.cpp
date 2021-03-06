/*ABIYAZ CHOWDHURY FALL 2017 CSE528 PROJECT
To run, ensure you have the dependencies in your main project folder:
- "includes" folder, which should contain "GL", "GLFW" and "glm". Within those folders the necessary headers should be present.
- "libraries" folder should contian all the libraries.
- the shaders need to be present as well.
- "dragon.obj" is the dragon model which needs to be present.
- "common" folder which should contain the object loader and camera controls
- Do not use precompiled headers.
*/
#include <iostream>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "FreeImage/FreeImage.h"
GLFWwindow* window;

int frameWidth, frameHeight;
int screenWidth = 1024;
int screenHeight = 768;
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
//#include <FreeImage.h>
using namespace glm;
using namespace std;
#include "common/shader.hpp"
#include "common/controls.hpp""
#include "common/objloader.hpp""

GLuint vertexbuffer, normalbuffer, programID, MatrixID, FARPLANE, NEARPLANE, ModelMatrixID, ViewMatrixID, LightID;
GLubyte *image;
std::vector<glm::vec3> vertices;
std::vector<glm::vec3> normals;
ofstream output;

//DISPLAY PARAMETERS:
int display_mode = 1;
float farplane_control = 0.09f;
float nearplane_control = -0.04f;

void gamma_correction(GLubyte* image, GLubyte* target, GLfloat brightness_factor, int image_width, int image_height, int num_channels) {
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				GLfloat value = image[index];
				value = (255*pow(value / 255, (1.0f) / brightness_factor));
				target[index] = (GLubyte)value;
			}
		}
	}
}

int print_image_rawbytes(GLubyte* image, char* output_filename, int image_width, int image_height, int num_channels) {
	output.open(output_filename);
	if (!output) {
		cerr << "The results file failed to open. Please try again.";
		return 0;
	}
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				output << (int)image[j * num_channels * frameWidth + i * num_channels + k] << " ";
			}
		}
		output << "\n";
	}
	output.close();
}

void save_image(GLubyte* image, char* output_filename, int image_width, int image_height, int num_channels) {
	FIBITMAP *bitmap = FreeImage_ConvertFromRawBits(image, image_width, image_height, num_channels * frameWidth, 24, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, FALSE);
	if (FreeImage_Save(FIF_BMP, bitmap, output_filename, 0))
		printf("Successfully saved!\n");
	else
		printf("Failed saving!\n");
	FreeImage_Unload(bitmap);
}

void compute_binary_mask(GLubyte* image, GLubyte* target, GLuint threshold, int image_width, int image_height, int num_channels) {
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				target[j * 3 * frameWidth + i * num_channels + k] = (image[j * num_channels * frameWidth + i * num_channels + k] > 0) ? 255 : 0;
			}
		}
	}
}

void normalize_input_image(GLubyte* image, GLubyte* mask, int image_width, int image_height, int num_channels) {
	GLubyte minimum = 255;
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				GLubyte value = image[j * num_channels * image_width + i * num_channels + k];
				if ( value > 0) {
					minimum = (value < minimum) ? value : minimum;
				}
			}
		}
	}
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				GLubyte image_value = image[index];
				GLubyte mask_value = (mask[index] == 255) ? 1 : 0;
				image[index] = mask_value * (image_value - minimum);
			}
		}
	}
}

void compute_x_gradient(GLubyte* image, GLubyte* target, int image_width, int image_height, int num_channels) {
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				GLint value = (i < image_width - 1) ? image[index+ num_channels]- image[index] : 0;
				target[index] = (GLubyte) ((value < 0) ? -value : value);
			}
		}
	}
}

void compute_y_gradient(GLubyte* image, GLubyte* target, int image_width, int image_height, int num_channels) {
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * 3 + k;
				GLint value = (j < image_height - 1) ? (GLint) image[index + num_channels*image_width] - (GLint) image[index] : 0;
				target[index] = (GLubyte) ((value < 0) ? -value : value);
			}
		}
	}
}

void compute_gradient_magnitude(GLubyte* x_gradient, GLubyte* y_gradient, GLubyte* target, int image_width, int image_height, int num_channels) {
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				int value =  pow((GLint)x_gradient[index], 2) + pow((GLint)y_gradient[index], 2);
				target[index] = (value > 255) ? 255 : value;
			}
		}
	}
}

void compute_mask_boundary(GLubyte* x_gradient, GLubyte* y_gradient, GLubyte* target, int image_width, int image_height, int num_channels) {
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				target[index] = ((x_gradient[index] == 255) || (y_gradient[index] == 255)) ? 0 : 255;
			}
		}
	}
}

//for the following function, "first" is treated as a binary mask with possible values 0 and 255
void compute_image_product(GLubyte* first, GLubyte* second, GLubyte* target, int image_width, int image_height, int num_channels) {
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				GLint value = ((first[index] == 255) ? 1 : 0) * second[index];
				target[index] = (value > 255) ? 255 : value;
			}
		}
	}
}

void detect_outliers(GLubyte* image, GLubyte* target, GLfloat tolerance, int image_width, int image_height, int num_channels) {
	//compute image_mean;
	GLfloat mean = 0;
	int count = 0;
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				if (image[index] > 0) {
					count++;
					mean += image[index];
				}
			}
		}
	}
	mean /= count;
	//compute image_variance;
	GLfloat std = 0;
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				if (image[index] > 0) {
					std += pow(((GLfloat)image[index])-mean, 2);
				}
			}
		}
	}
	std /= (GLfloat) count;
	std = pow(std, .5f);
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				if (image[index] > 0) {
					target[index] = (abs((GLfloat)image[index] - mean) > tolerance*std) ? 0 : 255;
				}else {
					target[index] = 255;
				}
			}
		}
	}
}

void compress_gradients(GLubyte* image, GLubyte* target, GLfloat a, GLfloat b,int image_width, int image_height, int num_channels) {
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				GLfloat value = (GLfloat)image[index];
				if (value == 0) {
					target[index] = 0;
				}
				else {
					GLfloat attenuation = (a / value) * pow(value / a, b);
					//cout << "value: " << value << "  attenuation: " << attenuation << endl;
					value *= attenuation;
					target[index] = (GLubyte)((value > 255) ? 255 : value);
					//if (target[index] != image[index]) {
					//	cout << "GOOD" << endl;
					//}
				}
			}
		}
	}
}

void compute_image_sum(GLubyte* first, GLubyte* second, GLubyte* target, int image_width, int image_height, int num_channels) {
	for (int i = 0; i < image_width; i++) {
		for (int j = 0; j < image_height; j++) {
			for (int k = 0; k < num_channels; k++) {
				int index = j * num_channels * image_width + i * num_channels + k;
				GLint value = first[index] + second[index];
				target[index] = (value > 255) ? 255 : value;
			}
		}
	}
}

void LoadShaders() {
	// Create and compile our GLSL program from the shaders
	if (display_mode == 0) {
		programID = LoadShaders("TransformVertexShader.vertexshader", "TextureFragmentShader.fragmentshader");
	}
	else if (display_mode == 1) {
		programID = LoadShaders("TransformVertexShader2.vertexshader", "TextureFragmentShader2.fragmentshader");
	}
	else if (display_mode == 2) {
		programID = LoadShaders("TransformVertexShader2.vertexshader", "TextureFragmentShader3.fragmentshader");
	}
}

int GLinit() {
	if (!glfwInit()) //test GLFW initialization
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return -1;
	}
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(screenWidth, screenHeight, "CSE528 Fall 2017 Dragon!", NULL, NULL); //create the window
	if (window == NULL) { //test for succesful window creation
		fprintf(stderr, "Failed to open GLFW window.\n");
		getchar();
		glfwTerminate();
		return -1;
	}//test for successful window
	glfwMakeContextCurrent(window);
	glfwGetFramebufferSize(window, &frameWidth, &frameHeight); //extract size of framebuffer, which is platform dependent
	glewExperimental = true; // Initialize glew
	if (glewInit() != GLEW_OK) { //test for succesful initialization of glew
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE); //enable escape key
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); //hide mouse and enable free movement
	glfwSetCursorPos(window, screenWidth / 2, screenHeight / 2); //center mouse
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); //background to black
	glEnable(GL_DEPTH_TEST); //enable depth test
	glDepthFunc(GL_LESS); //hidden surface removal
	glEnable(GL_CULL_FACE); //backface culling
	LoadShaders();
	return 0;
}

void drawScene() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(programID); //assign shaders

	computeMatricesFromInputs();
	glm::mat4 ProjectionMatrix = getProjectionMatrix();
	glm::mat4 ViewMatrix = getViewMatrix();
	glm::mat4 ModelMatrix = glm::mat4(1.0);

	glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

	glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
	glUniform1f(FARPLANE, farplane_control);
	glUniform1f(NEARPLANE, nearplane_control);
	glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
	glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);
	glm::vec3 lightPos = glm::vec3(0, 0.12, 4);
	glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glVertexAttribPointer(
		0,                  // attribute
		3,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
	);

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glVertexAttribPointer(
		1,                                // attribute
		3,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset
	);

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	glReadBuffer(GL_FRONT);
}

int main(void) {
	GLinit();
	GLuint VertexArrayID, framebuffername, color, depth;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
	MatrixID = glGetUniformLocation(programID, "MVP");
	FARPLANE = glGetUniformLocation(programID, "farplane");
	NEARPLANE = glGetUniformLocation(programID, "nearplane");
	ViewMatrixID = glGetUniformLocation(programID, "V");
	ModelMatrixID = glGetUniformLocation(programID, "M");

	bool res = loadOBJ("dragon3.obj", vertices, normals);

	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);

	glGenBuffers(1, &normalbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);

	LightID = glGetUniformLocation(programID, "LightPosition_worldspace");

	//Setup the framebuffer
	glGenFramebuffers(1, &framebuffername);

	glGenTextures(1, &color);
	glGenRenderbuffers(1, &depth);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffername);
	glBindTexture(GL_TEXTURE_2D, color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frameWidth, frameHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, frameWidth, frameHeight);
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);

	if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "Framebuffer error\n");
		getchar();
		return -1;
	}

	bool flag = true;
	do {
		glfwPollEvents();
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffername);
		drawScene();
		glReadBuffer(GL_FRONT);
		if (flag) {
			image = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			cout << "Rendering scene into Frame Buffer Object!" << endl;
			glReadPixels(0, 0, frameWidth, frameHeight, GL_RGB, GL_UNSIGNED_BYTE, image);
			printf("framebuffer width: %d, framebuffer height: %d \n", frameWidth, frameHeight);
			flag = false;
			print_image_rawbytes(image, "output.txt", frameWidth, frameHeight, 3);
			save_image(image, "original.bmp", frameWidth, frameHeight, 3);

			//compute the binary mask from the original height field
			cout << "Computing binary segmentation of image!" << endl;
			GLubyte *binary_mask = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_binary_mask(image, binary_mask, 1, frameWidth, frameHeight, 3);
			save_image(binary_mask, "binary_mask.bmp", frameWidth, frameHeight, 3);

			//normalize the image so that the smallest foreground pixel is a background pixel
			cout << "Normalizing image to match foreground/background segmentation!" << endl;
			normalize_input_image(image,binary_mask, frameWidth, frameHeight, 3);
			save_image(image, "normalized.bmp", frameWidth, frameHeight, 3);

			//compute image derivative in x axis
			cout << "Computing x-derivative of original image!" << endl;
			GLubyte *x_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_x_gradient(image, x_gradient, frameWidth, frameHeight, 3);
			save_image(x_gradient, "x_gradient.bmp", frameWidth, frameHeight, 3);

			cout << "Gamma correcting the above!" << endl;
			GLubyte *x_gradient_gamma2 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			gamma_correction(x_gradient, x_gradient_gamma2, 2, frameWidth, frameHeight, 3);
			save_image(x_gradient_gamma2, "x_gradient_gamma2.bmp", frameWidth, frameHeight, 3);

			//compute image derivative in y axis
			cout << "Computing y-derivative of original image!" << endl;
			GLubyte *y_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_y_gradient(image, y_gradient, frameWidth, frameHeight, 3);
			save_image(y_gradient, "y_gradient.bmp", frameWidth, frameHeight, 3);

			//compute gradient magnitude from image derivatives, and try with gamma correction
			cout << "Computing gradient magnitude of original image!" << endl;
			GLubyte *gradient_magnitude = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			GLubyte *gradient_magnitude_gamma2 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			GLubyte *gradient_magnitude_gamma10 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_gradient_magnitude(x_gradient, y_gradient, gradient_magnitude, frameWidth, frameHeight, 3);
			cout << "Gamma correcting the above with gamma = 2!" << endl;
			gamma_correction(gradient_magnitude, gradient_magnitude_gamma2, 2, frameWidth, frameHeight, 3);
			cout << "Gamma correcting the above with gamma = 10!" << endl;
			gamma_correction(gradient_magnitude, gradient_magnitude_gamma10, 10, frameWidth, frameHeight, 3);
			save_image(gradient_magnitude, "gradient_magnitude.bmp", frameWidth, frameHeight, 3);
			save_image(gradient_magnitude_gamma2, "gradient_magnitude_gamma2.bmp", frameWidth, frameHeight, 3);
			save_image(gradient_magnitude_gamma10, "gradient_magnitude_gamma10.bmp", frameWidth, frameHeight, 3);

			print_image_rawbytes(gradient_magnitude, "gradient.txt", frameWidth, frameHeight, 3);

			//compute derivative along x axis for binary mask
			cout << "Computing x-derivative of binary mask!" << endl;
			GLubyte *mask_x_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_x_gradient(binary_mask, mask_x_gradient, frameWidth, frameHeight, 3);
			save_image(mask_x_gradient, "mask_x_gradient.bmp", frameWidth, frameHeight, 3);

			//compute derivative along y axis for binary mask
			cout << "Computing y-derivative of binary mask!" << endl;
			GLubyte *mask_y_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_y_gradient(binary_mask, mask_y_gradient, frameWidth, frameHeight, 3);
			save_image(mask_y_gradient, "mask_y_gradient.bmp", frameWidth, frameHeight, 3);

			//compute binary mask boundary using the binary mask derivatives
			cout << "Computing boundary of binary mask!" << endl;
			GLubyte *mask_boundary = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_mask_boundary(mask_x_gradient,mask_y_gradient, mask_boundary, frameWidth, frameHeight, 3);
			save_image(mask_boundary, "mask_boundary.bmp", frameWidth, frameHeight, 3);

			//multiply original image's x derivative by the complement of the mask boundary
			cout << "Multiplying x-derivative by the complement of the mask boundary!" << endl;
			GLubyte *modified_x_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_image_product(mask_boundary, x_gradient, modified_x_gradient, frameWidth, frameHeight, 3);
			save_image(modified_x_gradient, "modified_x_gradient.bmp", frameWidth, frameHeight, 3);

			GLubyte *modified_x_gradient_gamma2 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			cout << "Gamma correcting the above!" << endl;
			gamma_correction(modified_x_gradient, modified_x_gradient_gamma2, 2, frameWidth, frameHeight, 3);
			save_image(modified_x_gradient_gamma2, "modified_x_gradient_gamma2.bmp", frameWidth, frameHeight, 3);
			print_image_rawbytes(modified_x_gradient, "modified_x_gradient.txt", frameWidth, frameHeight, 3);

			//multiply original image's y derivative by the complement of the mask boundary
			cout << "Multiplying y-derivative by the complement of the mask boundary!" << endl;
			GLubyte *modified_y_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_image_product(mask_boundary, y_gradient, modified_y_gradient, frameWidth, frameHeight, 3);
			save_image(modified_y_gradient, "modified_y_gradient.bmp", frameWidth, frameHeight, 3);

			GLubyte *x_outliers = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			cout << "Detecting outliers in x-derivative!" << endl;
			detect_outliers(modified_x_gradient, x_outliers, .6f, frameWidth, frameHeight, 3);
			GLubyte *modified2_x_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			cout << "Multiplying x-derivative to remove outliers!" << endl;
			compute_image_product(x_outliers, modified_x_gradient, modified2_x_gradient, frameWidth, frameHeight, 3);
			save_image(modified2_x_gradient, "modified2_x_gradient.bmp", frameWidth, frameHeight, 3);

			cout << "Gamma correcting the above!" << endl;
			GLubyte *modified2_x_gradient_gamma2 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			gamma_correction(modified2_x_gradient, modified2_x_gradient_gamma2, 2, frameWidth, frameHeight, 3);
			save_image(modified2_x_gradient_gamma2, "modified2_x_gradient_gamma2.bmp", frameWidth, frameHeight, 3);

			GLubyte *y_outliers = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			cout << "Detecting outliers in y-derivative!" << endl;
			detect_outliers(modified_y_gradient, y_outliers, .6f, frameWidth, frameHeight, 3);
			GLubyte *modified2_y_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			cout << "Multiplying y-derivative to remove outliers!" << endl;
			compute_image_product(y_outliers, modified_y_gradient, modified2_y_gradient, frameWidth, frameHeight, 3);
			save_image(modified2_y_gradient, "modified2_y_gradient.bmp", frameWidth, frameHeight, 3);

			GLubyte *modified2_y_gradient_gamma2 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			gamma_correction(modified2_y_gradient, modified2_y_gradient_gamma2, 2, frameWidth, frameHeight, 3);
			save_image(modified2_y_gradient_gamma2, "modified2_y_gradient_gamma2.bmp", frameWidth, frameHeight, 3);

			GLubyte *modified3_x_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			cout << "Compressing x-derivative!" << endl;
			compress_gradients(modified2_x_gradient, modified3_x_gradient, 7.0f , 0.5f, frameWidth, frameHeight, 3);
			save_image(modified3_x_gradient, "modified3_x_gradient.bmp", frameWidth, frameHeight, 3);

			GLubyte *modified3_x_gradient_gamma2 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			gamma_correction(modified3_x_gradient, modified3_x_gradient_gamma2, 2, frameWidth, frameHeight, 3);
			save_image(modified3_x_gradient_gamma2, "modified3_x_gradient_gamma2.bmp", frameWidth, frameHeight, 3);

			GLubyte *modified3_y_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			cout << "Compressing y-derivative!" << endl;
			compress_gradients(modified2_y_gradient, modified3_y_gradient, 7.0f, 0.5f, frameWidth, frameHeight, 3);
			save_image(modified3_y_gradient, "modified3_y_gradient.bmp", frameWidth, frameHeight, 3);

			GLubyte *modified3_y_gradient_gamma2 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			gamma_correction(modified3_y_gradient, modified3_y_gradient_gamma2, 2, frameWidth, frameHeight, 3);
			save_image(modified3_y_gradient_gamma2, "modified3_y_gradient_gamma2.bmp", frameWidth, frameHeight, 3);

			//TODO: Implement bilinear filter here

			GLubyte *x_second_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_x_gradient(modified3_x_gradient, x_second_gradient, frameWidth, frameHeight, 3);
			save_image(x_second_gradient, "x_second_gradient.bmp", frameWidth, frameHeight, 3);

			GLubyte *x_second_gradient_gamma2 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			gamma_correction(x_second_gradient, x_second_gradient_gamma2, 2, frameWidth, frameHeight, 3);
			save_image(x_second_gradient_gamma2, "x_second_gradient_gamma2.bmp", frameWidth, frameHeight, 3);

			GLubyte *y_second_gradient = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_y_gradient(modified3_y_gradient, y_second_gradient, frameWidth, frameHeight, 3);
			save_image(y_second_gradient, "y_second_gradient.bmp", frameWidth, frameHeight, 3);

			GLubyte *y_second_gradient_gamma2 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			gamma_correction(y_second_gradient, y_second_gradient_gamma2, 2, frameWidth, frameHeight, 3);
			save_image(y_second_gradient_gamma2, "y_second_gradient_gamma2.bmp", frameWidth, frameHeight, 3);

			GLubyte *laplacian = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			compute_image_sum(x_second_gradient, y_second_gradient, laplacian, frameWidth, frameHeight, 3);
			save_image(laplacian, "laplacian.bmp", frameWidth, frameHeight, 3);

			GLubyte *laplacian_gamma2 = (GLubyte*)malloc(frameWidth*frameHeight * 3);
			gamma_correction(laplacian, laplacian_gamma2, 2, frameWidth, frameHeight, 3);
			save_image(laplacian_gamma2, "laplacian_gamma2.bmp", frameWidth, frameHeight, 3);

			free(binary_mask);
			free(image);
			free(x_gradient);
			free(y_gradient);
			free(gradient_magnitude);
			free(mask_x_gradient);
			free(mask_y_gradient);
			free(mask_boundary);
			free(modified_x_gradient);
			free(modified_y_gradient);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		drawScene();
		glfwSwapBuffers(window);
	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);

	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &normalbuffer);
	glDeleteProgram(programID);
	glDeleteVertexArrays(1, &VertexArrayID);
	glDeleteTextures(1, &color);
	glDeleteTextures(1, &depth);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &framebuffername);
	glfwTerminate();
	return 0;
}


#include "textures.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Textures::Textures() {}

GLuint Textures::getTexture(int id) {
	return this->textures[id];
}

void Textures::loadTexture2D(const string texName, bool isPixelated) {

	int imgHeight, imgWidth, numOfColorChannels;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* imgBytes = stbi_load(texName.c_str(), &imgWidth, &imgHeight, &numOfColorChannels, 0);

	glGenTextures(1, &textures[this->current_index]);
	glBindTexture(GL_TEXTURE_2D, textures[this->current_index]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, isPixelated ? GL_NEAREST : GL_LINEAR); //nearest or linear
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, isPixelated ? GL_NEAREST : GL_LINEAR);
	//nearest is for pixelated and linear is for smooth

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	//float flatColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	//glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgWidth, imgHeight, 0, numOfColorChannels < 4 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, imgBytes);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, imgWidth, imgHeight, numOfColorChannels < 4 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, imgBytes);

	stbi_image_free(imgBytes);
	glBindTexture(GL_TEXTURE_2D, 0);
	this->current_index++;
}
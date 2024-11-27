#pragma once
#include "include/freeglut.h"
#include <vector>
#include <string>
using std::vector; using std::string;

class Textures {
public:
	Textures();  //just create
	Textures(const Textures&) = delete;
	GLuint getTexture(int id); //get texture by id
	void loadTexture2D(const string texName, bool isPixelated); //load specific texture
private:
	const unsigned int size = 20;
	GLuint textures[20]{};
	unsigned int current_index = 0;
};
#pragma once

#include <SDL.h>
#include "WorldCamera.hpp"

struct SDLRenderer
{
	SDLRenderer(SDL_Window *window, int width, int height)
		: width(width), height(height)
	{
		framebuffer = new unsigned int[width * height];
		camera = new WorldCamera();

		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		mainTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
										SDL_TEXTUREACCESS_STREAMING, width, height);

		camera->aspect = (float)width / height;
		camera->fov = 45.0f;

		setupMatrices();

		// buildTriangle();
		buildCube();
	}

	~SDLRenderer()
	{
		delete[] framebuffer;
		SDL_DestroyTexture(mainTexture);
		SDL_DestroyRenderer(renderer);
	}

	void drawPoint(int x, int y, int color)
	{
		if (x >= width || x < 0)
			return;
		if (y >= height || y < 0)
			return;

		framebuffer[x + y * width] = color;
	}

	/**
	 * Draw line with Bresenham algorithm
	 */
	void drawLine(const Vector2 &startPos, const Vector2 &endPos, int color)
	{
		auto drawLow = [this](int x0, int y0, int x1, int y1, int color)
		{
			int dx = x1 - x0, dy = y1 - y0;
			int yi = 1;
			if (dy < 0)
			{
				yi = -1;
				dy = -dy;
			}
			int d = (2 * dy) - dx;
			int y = y0;

			for (int x = x0; x < x1; ++x)
			{
				drawPoint(x, y, color);
				if (d > 0)
				{
					y = y + yi;
					d = d + (2 * (dy - dx));
				}
				else
				{
					d = d + 2 * dy;
				}
			}
		};

		auto drawHigh = [this](int x0, int y0, int x1, int y1, int color)
		{
			int dx = x1 - x0, dy = y1 - y0;
			int xi = 1;
			if (dx < 0)
			{
				xi = -1;
				dx = -dx;
			}
			int d = (2 * dx) - dy;
			int x = x0;

			for (int y = y0; y < y1; ++y)
			{
				drawPoint(x, y, color);
				if (d > 0)
				{
					x = x + xi;
					d = d + (2 * (dx - dy));
				}
				else
				{
					d = d + 2 * dx;
				}
			}
		};

		if (abs(endPos.y - startPos.y) < abs(endPos.x - startPos.x))
		{
			if (startPos.x > endPos.x)
			{
				drawLow(endPos.x, endPos.y, startPos.x, startPos.y, color);
			}
			else
			{
				drawLow(startPos.x, startPos.y, endPos.x, endPos.y, color);
			}
		}
		else
		{
			if (startPos.y > endPos.y)
			{
				drawHigh(endPos.x, endPos.y, startPos.x, startPos.y, color);
			}
			else
			{
				drawHigh(startPos.x, startPos.y, endPos.x, endPos.y, color);
			}
		}
	}

	void buildTriangle()
	{
		// Build some vertices for drawing triangle
		triVerts[0] = {-1.0f, -1.0f, +0.0f};
		triVerts[1] = {+1.0f, -1.0f, +0.0f};
		triVerts[2] = {+0.0f, +1.0f, +0.0f};
	}

	void renderTriangleLines()
	{
		Matrix4x4 rotateMat = Matrix4x4::identity;
		rotateRadian += 0.06f;
		rotateMat.rotateY(rotateRadian);

		Vector3 tri[3];
		for (int i = 0; i < 3; ++i)
		{
			triVerts[i] = rotateMat * triVerts[i];
			Vector4 v = {triVerts[i].x, triVerts[i].y, triVerts[i].z, 1.0f};
			transformToScreen(v);
			tri[i].x = v.x, tri[i].y = v.y, tri[i].z = v.z;
		}

		const int whiteColor = 0xFFFFFFFF;
		drawLine({tri[0].x, tri[0].y}, {tri[1].x, tri[1].y}, whiteColor);
		drawLine({tri[1].x, tri[1].y}, {tri[2].x, tri[2].y}, whiteColor);
		drawLine({tri[0].x, tri[0].y}, {tri[2].x, tri[2].y}, whiteColor);
	}

	void buildCube()
	{
		// Build some vertices for drawing cube
		cubeVerts[0] = {-1.0f, -1.0f, -1.0f};
		cubeVerts[1] = {+1.0f, -1.0f, -1.0f};
		cubeVerts[2] = {+1.0f, +1.0f, -1.0f};
		cubeVerts[3] = {-1.0f, +1.0f, -1.0f};

		cubeVerts[4] = {-1.0f, -1.0f, +1.0f};
		cubeVerts[5] = {+1.0f, -1.0f, +1.0f};
		cubeVerts[6] = {+1.0f, +1.0f, +1.0f};
		cubeVerts[7] = {-1.0f, +1.0f, +1.0f};
	}

	void renderCubeLines()
	{
		Matrix4x4 rotateMat = Matrix4x4::identity;
		rotateRadian += 0.06f;
		rotateMat.rotateY(rotateRadian);

		Vector3 cube[8];
		for (int i = 0; i < 8; ++i)
		{
			cubeVerts[i] = rotateMat * cubeVerts[i];
			Vector4 v = {cubeVerts[i].x, cubeVerts[i].y, cubeVerts[i].z, 1.0f};
			transformToScreen(v);
			cube[i].x = v.x, cube[i].y = v.y, cube[i].z = v.z;
		}

		const int whiteColor = 0xFFFFFFFF;
		drawLine({cube[0].x, cube[0].y}, {cube[1].x, cube[1].y}, whiteColor);
		drawLine({cube[1].x, cube[1].y}, {cube[2].x, cube[2].y}, whiteColor);
		drawLine({cube[2].x, cube[2].y}, {cube[3].x, cube[3].y}, whiteColor);
		drawLine({cube[3].x, cube[3].y}, {cube[0].x, cube[0].y}, whiteColor);

		drawLine({cube[4].x, cube[4].y}, {cube[5].x, cube[5].y}, whiteColor);
		drawLine({cube[5].x, cube[5].y}, {cube[6].x, cube[6].y}, whiteColor);
		drawLine({cube[6].x, cube[6].y}, {cube[7].x, cube[7].y}, whiteColor);
		drawLine({cube[7].x, cube[7].y}, {cube[4].x, cube[4].y}, whiteColor);

		drawLine({cube[0].x, cube[0].y}, {cube[4].x, cube[4].y}, whiteColor);
		drawLine({cube[1].x, cube[1].y}, {cube[5].x, cube[5].y}, whiteColor);
		drawLine({cube[2].x, cube[2].y}, {cube[6].x, cube[6].y}, whiteColor);
		drawLine({cube[3].x, cube[3].y}, {cube[7].x, cube[7].y}, whiteColor);
	}

	void transformToScreen(Vector4 &point)
	{
		point = projectionMatrix * (cameraMatrix * point);
		point.perspectiveDivide();
		point = viewportMatrix * point;
	}

	void setupMatrices()
	{
		math::setupCameraMatrix(cameraMatrix, camera->eye, camera->at, camera->up);
		math::setupPerspectiveProjectionMatrix(projectionMatrix, camera->fov,
											   camera->aspect, kZNear, kZFar);
		math::setupViewportMatrix(viewportMatrix, 0, 0, width, height, kZNear, kZFar);
	}

	void render(double delta)
	{
		memset((char *)framebuffer, 0, sizeof(int) * width * height);

		// renderTriangleLines();
		renderCubeLines();

		SDL_UpdateTexture(mainTexture, nullptr, framebuffer, width * 4);
		SDL_RenderCopy(renderer, mainTexture, nullptr, nullptr);
		SDL_RenderPresent(renderer);
		SDL_Delay(16);
	}

	int width;
	int height;
	unsigned int *framebuffer;
	WorldCamera *camera;

	Matrix4x4 viewportMatrix, projectionMatrix, cameraMatrix;

	SDL_Renderer *renderer;
	SDL_Texture *mainTexture;

	const float kZNear = 0.1f, kZFar = 10.0f;

	float rotateRadian{0.0f};

	// Start Draw Tri
	Vector3 triVerts[3];
	// End Draw Tri

	// Start Draw Cube
	Vector3 cubeVerts[8];
	// End Draw Cube
};

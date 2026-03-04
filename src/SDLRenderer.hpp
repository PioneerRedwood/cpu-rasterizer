#pragma once

#include <algorithm>
#include <SDL.h>
#include "WorldCamera.hpp"
#include "TextureLoader.hpp"

class SDLRenderer
{
public:
	SDLRenderer(SDL_Window *window, int width, int height)
		: width(width), height(height)
	{
		framebuffer = new unsigned int[width * height];
		camera = new WorldCamera();
		zbuffer = new float[width * height];
		std::fill(zbuffer, zbuffer + width * height, 1.0f);

		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		mainTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
										SDL_TEXTUREACCESS_STREAMING, width, height);

		camera->aspect = (float)width / height;
		camera->fov = 45.0f;

		setupMatrices();

		buildTriangle();
		buildCube();

		textureLoader = new TextureLoader("resources");
		tgaTexture = textureLoader->loadTGATextureWithName(renderer, "keep-carm.tga");
		if (tgaTexture == nullptr) {
			Logf("Failed to load texture");
		}
	}

	~SDLRenderer()
	{
		delete[] framebuffer;
		delete[] zbuffer;
		delete camera;
		delete textureLoader;
		SDL_DestroyTexture(mainTexture);
		SDL_DestroyRenderer(renderer);
	}

	void render(double delta)
	{
		memset((char *)framebuffer, 0, sizeof(int) * width * height);
		std::fill(zbuffer, zbuffer + width * height, 1.0f);

		renderTriangle();
		// renderCubeLines();

		SDL_UpdateTexture(mainTexture, nullptr, framebuffer, width * 4);
		SDL_RenderCopy(renderer, mainTexture, nullptr, nullptr);
		SDL_RenderPresent(renderer);
		SDL_Delay(32);
	}

private:
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

	void buildTriangle()
	{
		// Build some vertices for drawing triangle
		triVerts[0] = {-1.0f, -1.0f, +0.0f};
		triVerts[1] = {+1.0f, -1.0f, +0.0f};
		triVerts[2] = {+0.0f, +1.0f, +0.0f};
	}

	void renderTriangle()
	{
		Matrix4x4 rotateMat = Matrix4x4::identity;
		rotateRadian += 0.6f;
		rotateMat.rotateY(rotateRadian);

		Vector3 tri[3];
		for (int i = 0; i < 3; ++i)
		{
			const Vector3 rotated = rotateMat * triVerts[i];
			Vector4 v = {rotated.x, rotated.y, rotated.z, 1.0f};
			transformToScreen(v);
			tri[i].x = v.x, tri[i].y = v.y, tri[i].z = v.z;
		}

		 const int fillColor = 0xFF33AAFF;

		// #1
		// fillTriangleBarycentric(tri[0], tri[1], tri[2], fillColor);
		
		// #2
		 drawTri(tri[0], tri[1], tri[2], fillColor, true);
		
		// #3
		//fillTriangleTexture(tri[0], tri[1], tri[2], tgaTexture->pixelData());

		// Draw edges
		const int whiteColor = 0xFFFFFFFF;
		drawLine({tri[0].x, tri[0].y}, {tri[1].x, tri[1].y}, whiteColor);
		drawLine({tri[1].x, tri[1].y}, {tri[2].x, tri[2].y}, whiteColor);
		drawLine({tri[0].x, tri[0].y}, {tri[2].x, tri[2].y}, whiteColor);
	}

	void fillTriangleBarycentric(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, int color)
	{
		const float minX = std::min({v0.x, v1.x, v2.x});
		const float maxX = std::max({v0.x, v1.x, v2.x});
		const float minY = std::min({v0.y, v1.y, v2.y});
		const float maxY = std::max({v0.y, v1.y, v2.y});

		const int x0 = std::max(0, (int)std::floor(minX));
		const int y0 = std::max(0, (int)std::floor(minY));
		const int x1 = std::min(width - 1, (int)std::ceil(maxX));
		const int y1 = std::min(height - 1, (int)std::ceil(maxY));

		const Vector2 p0{v0.x, v0.y};
		const Vector2 p1{v1.x, v1.y};
		const Vector2 p2{v2.x, v2.y};
		const float area = math::edgeFunction(p0, p1, v2.x, v2.y);
		if (std::abs(area) < 1e-6f)
		{
			return;
		}

		for (int y = y0; y <= y1; ++y)
		{
			for (int x = x0; x <= x1; ++x)
			{
				const float px = (float)x + 0.5f;
				const float py = (float)y + 0.5f;

				const float w0 = math::edgeFunction(p1, p2, px, py);
				const float w1 = math::edgeFunction(p2, p0, px, py);
				const float w2 = math::edgeFunction(p0, p1, px, py);

				const bool insideCCW = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
				const bool insideCW = (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
				if ((area > 0.0f && insideCCW) || (area < 0.0f && insideCW))
				{
					drawPoint(x, y, color);
				}
			}
		}
	}

	void drawTri(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, int color, bool cullBackface = true)
	{
		const float minX = std::min({v0.x, v1.x, v2.x});
		const float maxX = std::max({v0.x, v1.x, v2.x});
		const float minY = std::min({v0.y, v1.y, v2.y});
		const float maxY = std::max({v0.y, v1.y, v2.y});

		const int x0 = std::max(0, (int)std::floor(minX));
		const int y0 = std::max(0, (int)std::floor(minY));
		const int x1 = std::min(width - 1, (int)std::ceil(maxX));
		const int y1 = std::min(height - 1, (int)std::ceil(maxY));

		const Vector2 p0{v0.x, v0.y};
		const Vector2 p1{v1.x, v1.y};
		const Vector2 p2{v2.x, v2.y};
		const float area = math::edgeFunction(p0, p1, v2.x, v2.y);
		if (std::abs(area) < 1e-6f)
		{
			return;
		}

		// Back-face culling
		if (cullBackface && area > 0.0f)
		{
			return;
		}

		for (int y = y0; y <= y1; ++y)
		{
			for (int x = x0; x <= x1; ++x)
			{
				const float px = (float)x + 0.5f;
				const float py = (float)y + 0.5f;

				const float w0 = math::edgeFunction(p1, p2, px, py);
				const float w1 = math::edgeFunction(p2, p0, px, py);
				const float w2 = math::edgeFunction(p0, p1, px, py);

				const bool insideCCW = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
				const bool insideCW = (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);

				const bool checkInside = cullBackface
											 ? insideCW
											 : ((area > 0.0f && insideCCW) || (area < 0.0f && insideCW));

				if (checkInside)
				{
					// Zbuffer test
					const float invArea = 1.0f / area;
					const float b0 = w0 * invArea;
					const float b1 = w1 * invArea;
					const float b2 = w2 * invArea;

					const float z = b0 * v0.z + b1 * v1.z + b2 * v2.z;
					if (z < 0.0f || z > 1.0f)
						continue;

					const int idx = x + y * width;
					if (z < zbuffer[idx])
					{
						zbuffer[idx] = z;
						framebuffer[idx] = color;
					}
				}
			}
		}
	}

	void buildCube()
	{
		// Build some vertices for drawing cube
		cubeVerts[0] = {-1.0f, -1.0f, -1.0f};
		cubeVerts[1] = {-1.0f, +1.0f, -1.0f};
		cubeVerts[2] = {+1.0f, +1.0f, -1.0f};
		cubeVerts[3] = {+1.0f, -1.0f, -1.0f};

		cubeVerts[4] = {-1.0f, -1.0f, +1.0f};
		cubeVerts[5] = {-1.0f, +1.0f, +1.0f};
		cubeVerts[6] = {+1.0f, +1.0f, +1.0f};
		cubeVerts[7] = {+1.0f, -1.0f, +1.0f};
	}

	void renderCubeLines()
	{
		Matrix4x4 rotateMat = Matrix4x4::identity;
		rotateRadian += 0.06f;
		rotateMat.rotateY(rotateRadian);

		Vector3 cube[8];
		for (int i = 0; i < 8; ++i)
		{
			const Vector3 rotated = rotateMat * cubeVerts[i];
			Vector4 v = {rotated.x, rotated.y, rotated.z, 1.0f};
			transformToScreen(v);
			cube[i].x = v.x, cube[i].y = v.y, cube[i].z = v.z;
		}

		const int fillColor = 0XFF33AAFF;
		// Front
		drawTri(cube[0], cube[1], cube[2], fillColor, true);
		drawTri(cube[0], cube[2], cube[3], fillColor, true);
		//Back
		drawTri(cube[4], cube[5], cube[6], fillColor, true);
		drawTri(cube[4], cube[6], cube[7], fillColor, true);
		// Top
		drawTri(cube[1], cube[5], cube[6], fillColor, true);
		drawTri(cube[1], cube[6], cube[7], fillColor, true);
		// Bottom
		drawTri(cube[0], cube[4], cube[7], fillColor, true);
		drawTri(cube[0], cube[7], cube[3], fillColor, true);
		// Right
		drawTri(cube[3], cube[2], cube[6], fillColor, true);
		drawTri(cube[3], cube[6], cube[7], fillColor, true);
		// Left
		drawTri(cube[0], cube[1], cube[5], fillColor, true);
		drawTri(cube[0], cube[5], cube[4], fillColor, true);
		
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

	void fillTriangleTexture(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, const RGBA* bitmap) {
		if (bitmap == nullptr || tgaTexture == nullptr)
		{
			return;
		}

		const TGAHeader *header = tgaTexture->header();
		const int texWidth = (int)header->width;
		const int texHeight = (int)header->height;
		if (texWidth <= 0 || texHeight <= 0)
		{
			return;
		}

		const float minX = std::min({v0.x, v1.x, v2.x});
		const float maxX = std::max({v0.x, v1.x, v2.x});
		const float minY = std::min({v0.y, v1.y, v2.y});
		const float maxY = std::max({v0.y, v1.y, v2.y});

		const int x0 = std::max(0, (int)std::floor(minX));
		const int y0 = std::max(0, (int)std::floor(minY));
		const int x1 = std::min(width - 1, (int)std::ceil(maxX));
		const int y1 = std::min(height - 1, (int)std::ceil(maxY));
		if (x0 > x1 || y0 > y1)
		{
			return;
		}

		const Vector2 p0{v0.x, v0.y};
		const Vector2 p1{v1.x, v1.y};
		const Vector2 p2{v2.x, v2.y};
		const float area = math::edgeFunction(p0, p1, v2.x, v2.y);
		if (std::abs(area) < 1e-6f)
		{
			return;
		}

		if (area > 0.0f)
		{
			return;
		}

		const float invArea = 1.0f / area;
		const int texMaxX = texWidth - 1;
		const int texMaxY = texHeight - 1;

		const float stepW0X = p2.y - p1.y;
		const float stepW1X = p0.y - p2.y;
		const float stepW2X = p1.y - p0.y;

		const float stepW0Y = p1.x - p2.x;
		const float stepW1Y = p2.x - p0.x;
		const float stepW2Y = p0.x - p1.x;

		const float startX = (float)x0 + 0.5f;
		const float startY = (float)y0 + 0.5f;

		float w0Row = math::edgeFunction(p1, p2, startX, startY);
		float w1Row = math::edgeFunction(p2, p0, startX, startY);
		float w2Row = math::edgeFunction(p0, p1, startX, startY);

		for (int y = y0; y <= y1; ++y)
		{
			float w0 = w0Row;
			float w1 = w1Row;
			float w2 = w2Row;
			int idx = x0 + y * width;

			for (int x = x0; x <= x1; ++x, ++idx)
			{
				if (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f)
				{
					const float b0 = w0 * invArea;
					const float b1 = w1 * invArea;
					const float b2 = w2 * invArea;

					const float z = b0 * v0.z + b1 * v1.z + b2 * v2.z;
					if (z >= 0.0f && z <= 1.0f && z < zbuffer[idx])
					{
						zbuffer[idx] = z;

						float u = b1 + (0.5f * b2);
						float v = b0 + b1;
						u = std::max(0.0f, std::min(1.0f, u));
						v = std::max(0.0f, std::min(1.0f, v));

						const int texX = std::min(texMaxX, std::max(0, (int)(u * (float)texMaxX + 0.5f)));
						const int texY = std::min(texMaxY, std::max(0, (int)(v * (float)texMaxY + 0.5f)));
						const RGBA &sample = bitmap[texX + texY * texWidth];

						framebuffer[idx] = ((unsigned int)sample.a << 24) |
										   ((unsigned int)sample.b << 16) |
										   ((unsigned int)sample.g << 8) |
										   (unsigned int)sample.r;
					}
				}

				w0 += stepW0X;
				w1 += stepW1X;
				w2 += stepW2X;
			}

			w0Row += stepW0Y;
			w1Row += stepW1Y;
			w2Row += stepW2Y;
		}
	}

	// TODO: Bilinear sampling

	// TODO: Other shaders

private:
	int width{0};
	int height{0};
	unsigned int *framebuffer{nullptr};
	WorldCamera *camera{nullptr};

	Matrix4x4 viewportMatrix, projectionMatrix, cameraMatrix;

	SDL_Renderer *renderer{nullptr};
	SDL_Texture *mainTexture{nullptr};
	TextureLoader *textureLoader {nullptr};
	float *zbuffer{nullptr};

	const float kZNear{0.1f}, kZFar{10.0f};

	float rotateRadian{0.0f};

	// Start Draw Tri
	Vector3 triVerts[3];
	// End Draw Tri

	// Start Draw Cube
	Vector3 cubeVerts[8];
	// End Draw Cube

	// Start Draw Textured Tri
	TGA* tgaTexture { nullptr };
	// End Draw Textured Tri
};

﻿/*
 * esmini - Environment Simulator Minimalistic
 * https://github.com/esmini/esmini
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) partners of Simulation Scenarios
 * https://sites.google.com/view/simulationscenarios
 */

#include <stdarg.h>
#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <sstream>
#include <locale>
#include <array>


// UDP network includes
#ifndef _WIN32
	 /* Assume that any non-Windows platform uses POSIX-style sockets instead. */
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
	#include <unistd.h> /* Needed for close() */
#else
	#include <winsock2.h>
	#include <Ws2tcpip.h>
#endif

#include "CommonMini.hpp"


// #define DEBUG_TRACE

 // These variables are autogenerated and compiled
 // into the library by the version.cmake script
extern const char* ESMINI_GIT_TAG;
extern const char* ESMINI_GIT_REV;
extern const char* ESMINI_GIT_BRANCH;
extern const char* ESMINI_BUILD_VERSION;

static SE_SystemTime systemTime_;
static const int max_csv_entry_length = 1024;

// Fallback list of 3D models where model_id is index in list
static const char* entityModelsFilesFallbackList_[] =
{
	"car_white.osgb",
	"car_blue.osgb",
	"car_red.osgb",
	"car_yellow.osgb",
	"truck_yellow.osgb",
	"van_red.osgb",
	"bus_blue.osgb",
	"walkman.osgb",
	"moose_cc0.osgb",
	"cyclist.osgb",
	"mc.osgb",
	"car_trailer.osgb",
	"semi_tractor.osgb",
	"semi_trailer.osgb",
	"truck_trailer.osgb",
};


const char* esmini_git_tag(void)
{
	return ESMINI_GIT_TAG;
}

const char* esmini_git_rev(void)
{
	return ESMINI_GIT_REV;
}

const char* esmini_git_branch(void)
{
	return ESMINI_GIT_BRANCH;
}

const char* esmini_build_version(void)
{
	return ESMINI_BUILD_VERSION;
}

std::map<int, std::string> ParseModelIds()
{
	std::map<int, std::string> entity_model_map;

	const std::string filename = "model_ids.txt";

	// find and open model_ids.txt file. Test some paths.
	std::vector<std::string> file_name_candidates;

	file_name_candidates.push_back(filename);

	// Check registered paths
	for (size_t i = 0; i < SE_Env::Inst().GetPaths().size(); i++)
	{
		file_name_candidates.push_back(CombineDirectoryPathAndFilepath(SE_Env::Inst().GetPaths()[i], filename));
		file_name_candidates.push_back(CombineDirectoryPathAndFilepath(SE_Env::Inst().GetPaths()[i] + "/../resources", filename));
	}

	size_t i;
	for (i = 0; i < file_name_candidates.size(); i++)
	{
		if (FileExists(file_name_candidates[i].c_str()))
		{
			std::ifstream infile(file_name_candidates[i]);
			if (infile.is_open())
			{
				int id;
				std::string model3d;
				while (infile >> id >> model3d)
				{
					entity_model_map[id] = model3d;
				}
				break;
			}
			infile.close();
		}
	}

	if (i == file_name_candidates.size())
	{
		LOG("Failed to load %s file. Tried:", filename.c_str());
		for (unsigned int j = 0; j < file_name_candidates.size(); j++)
		{
			LOG("  %s", file_name_candidates[j].c_str());
		}

		printf("  continue with internal hard coded list: \n");
		for (int j = 0; static_cast<unsigned int>(j) < sizeof(entityModelsFilesFallbackList_) / sizeof(char*); j++)
		{
			entity_model_map[j] = entityModelsFilesFallbackList_[j];
			LOG("    %2d: %s", j, entity_model_map[j].c_str());
		}
	}

	return entity_model_map;
}

std::string ControlDomain2Str(ControlDomains domains)
{
	if (domains == ControlDomains::DOMAIN_BOTH)
	{
		return "lateral and longitudinal";
	}
	else if (domains == ControlDomains::DOMAIN_LAT)
	{
		return "lateral";
	}
	else if (domains == ControlDomains::DOMAIN_LONG)
	{
		return "longitudinal";
	}

	return "none";
}

bool FileExists(const char* fileName)
{
	std::ifstream infile(fileName);
	return infile.good();
}

std::string CombineDirectoryPathAndFilepath(std::string dir_path, std::string file_path)
{
	std::string path = file_path;

	if (file_path[0] != '/' || file_path[0] != '\\' || file_path[1] != ':')
	{
		// Relative path. Make sure it starts with ".." or "./"
		if (path[0] != '.')
		{
			path.insert(0, "./");
		}
		if (dir_path != "")
		{
			// Combine with directory path
			path.insert(0, dir_path + '/');
		}
	}

	return path;
}

double GetAngleOfVector(double x, double y)
{
	double angle;
	if (abs(x) < SMALL_NUMBER)
	{
		if (abs(y) < SMALL_NUMBER)
		{
			return 0.0; // undefined
		}
		x = SIGN(x) * SMALL_NUMBER;
	}
	angle = atan2(y, x);
	if (angle < 0.0)
	{
		angle += 2*M_PI;
	}
	return angle;
}

double GetAbsAngleDifference(double angle1, double angle2)
{
	double diff = fmod(angle1 - angle2, 2 * M_PI);

	if (diff < 0)
	{
		diff += 2 * M_PI;
	}

	if (diff > M_PI)
	{
		diff = 2 * M_PI - diff;
	}

	return GetAngleInInterval2PI(diff);
}

double GetAngleDifference(double angle1, double angle2)
{
	double diff = fmod(angle1 - angle2, 2 * M_PI);

	if (diff < -M_PI)
	{
		diff += 2 * M_PI;
	}
	else if (diff > M_PI)
	{
		diff -= 2 * M_PI;
	}

	return diff;
}

bool IsAngleForward(double teta)
{
	return !(teta > M_PI_2 && teta < 3 * M_PI_2);
}

double GetAngleSum(double angle1, double angle2)
{
	return GetAngleInInterval2PI(angle1 + angle2);
}

double GetAngleInInterval2PI(double angle)
{
	double angle2 = fmod(angle, 2 * M_PI);

	if (angle2 < 0)
	{
		angle2 += 2 * M_PI;
	}
	else if (angle2 == -0)
	{
		angle2 = 0;
	}

	return angle2;
}

double GetAngleInIntervalMinusPIPlusPI(double angle)
{
	double angle2 = fmod(angle, 2 * M_PI);

	if (angle2 < -M_PI)
	{
		angle2 += 2 * M_PI;
	}
	else if (angle2 > M_PI)
	{
		angle2 -= 2 * M_PI;
	}

	return angle2;
}

int GetIntersectionOfTwoLineSegments(double ax1, double ay1, double ax2, double ay2, double bx1, double by1, double bx2, double by2, double &x3, double &y3)
{
	// Inspiration: https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection

	double t_demonitator = (ax1 - ax2) * (by1 - by2) - (ay1 - ay2) * (bx1 - bx2);

	if(fabs(t_demonitator) < SMALL_NUMBER)
	{
		return -1;
	}

	double t = ((ax1 - bx1) * (by1 - by2) - (ay1 - by1) * (bx1 - bx2)) / t_demonitator;

	x3 = ax1 + t * (ax2 - ax1);
	y3 = ay1 + t * (ay2 - ay1);

	return 0;
}

bool PointInBetweenVectorEndpoints(double x3, double y3, double x1, double y1, double x2, double y2, double &sNorm)
{
	bool inside;

	if (fabs(y2 - y1) < SMALL_NUMBER && fabs(x2 - x1) < SMALL_NUMBER)
	{
		// Point - not really a line
		// Not sure if true of false should be returned
		sNorm = 0;
		inside = true;
	}
	else if (fabs(x2 - x1) < fabs(y2 - y1))  // Line is steep (more vertical than horizontal
	{
		sNorm = (y3 - y1) / (y2 - y1);
		if (y2 > y1)  // ascending
		{
			inside = !(y3 < y1 || y3 > y2);
		}
		else
		{
			inside = !(y3 > y1 || y3 < y2);
		}
	}
	else
	{
		sNorm = (x3 - x1) / (x2 - x1);
		if (x2 > x1)  // forward
		{
			inside = !(x3 < x1 || x3 > x2);
		}
		else
		{
			inside = !(x3 > x1 || x3 < x2);
		}
	}
	if (!inside)
	{
		if (sNorm < 0)
		{
			sNorm = -PointDistance2D(x3, y3, x1, y1);
		}
		else
		{
			sNorm = PointDistance2D(x3, y3, x2, y2);
		}
	}
	return inside;
}

double DistanceFromPointToEdge2D(double x3, double y3, double x1, double y1, double x2, double y2, double* x, double* y)
{
	double px = 0;
	double py = 0;
	double distance = 0;
	double sNorm = 0;

	// First project point on edge
	ProjectPointOnVector2D(x3, y3, x1, y1, x2, y2, px, py);
	distance = PointDistance2D(x3, y3, px, py);

	if (PointInBetweenVectorEndpoints(px, py, x1, y1, x2, y2, sNorm))
	{
		// Point within edge interior
		if (x) *x = px;
		if (y) *y = py;
	}
	else if (sNorm < 0)
	{
		// measure to first endpoint
		distance = PointDistance2D(x3, y3, x1, y1);
		if (x) *x = x1;
		if (y) *y = y1;
	}
	else
	{
		// measure to other (2:nd) endpoint
		distance = PointDistance2D(x3, y3, x2, y2);
		if (x) *x = x2;
		if (y) *y = y2;
	}

	return distance;
}

double DistanceFromPointToLine2D(double x3, double y3, double x1, double y1, double x2, double y2, double* x, double* y)
{
	double distance = 0;
	double xp = 0.0, yp = 0.0;

	// project point on edge, and measure distance to that point
	if (x == nullptr) x = &xp;
	if (y == nullptr) y = &yp;
	ProjectPointOnVector2D(x3, y3, x1, y1, x2, y2, *x, *y);
	distance = PointDistance2D(x3, y3, *x, *y);

	return distance;
}

double DistanceFromPointToLine2DWithAngle(double x3, double y3, double x1, double y1, double angle)
{
	// https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
	return abs(cos(angle) * (y1 - y3) - sin(angle) * (x1 - x3));
}

int PointSideOfVec(double px, double py, double vx1, double vy1, double vx2, double vy2)
{
	// Use cross product
	return SIGN(GetCrossProduct2D((vx2 - vx1), (px - vx1), (vy2 - vy1), (py - vy1)));
}

double PointDistance2D(double x0, double y0, double x1, double y1)
{
	return sqrt((x1 - x0)*(x1 - x0) + (y1 - y0) * (y1 - y0));
}

double PointToLineDistance2DSigned(double px, double py, double lx0, double ly0, double lx1, double ly1)
{
	double l0x = lx1 - lx0;
	double l0y = ly1 - ly0;
	double cp = GetCrossProduct2D(lx1 - lx0, ly1 - ly0, px - lx0, py - ly0);
	double l0Length = sqrt(l0x * l0x + l0y * l0y);
	return cp / l0Length;
}

double PointSquareDistance2D(double x0, double y0, double x1, double y1)
{
	return (x1 - x0)*(x1 - x0) + (y1 - y0) * (y1 - y0);
}

double PointHeadingDistance2D(double x0, double y0, double h, double x1, double y1)
{
	(void)h;
	return (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
}

void ProjectPointOnVector2D(double x, double y, double vx1, double vy1, double vx2, double vy2, double &px, double &py)
{
	// Project the given point on the straight line between geometry end points
	// https://stackoverflow.com/questions/1811549/perpendicular-on-a-line-from-a-given-point

	double dx = vx2 - vx1;
	double dy = vy2 - vy1;

	if (fabs(dx) < SMALL_NUMBER && fabs(dy) < SMALL_NUMBER)
	{
		// Line too small - projection not possible, copy first point position
		px = vx1;
		py = vy1;
	}
	else
	{
		double k = (dy * (x - vx1) - dx * (y - vy1)) / (dy*dy + dx*dx);
		px = x - k * dy;
		py = y + k * dx;
	}
}

bool IsPointWithinSectorBetweenTwoLines(SE_Vector p, SE_Vector l0p0, SE_Vector l0p1, SE_Vector l1p0, SE_Vector l1p1, double& sNorm)
{
	// If point is on the right side of first normal and to the left side of the second normal, then it's in between else not
	double d0 = (p - l0p0).Cross(l0p1 - l0p0);
	double d1 = (p - l1p0).Cross(l1p1 - l1p0);

	double x = 0.0, y = 0.0;

	// Find factor between 0,1 how close the point is the first (0) vs second point (1)
	double dist0 = DistanceFromPointToLine2D(p.x(), p.y(), l0p0.x(), l0p0.y(), l0p1.x(), l0p1.y(), &x, &y);
	double dist1 = DistanceFromPointToLine2D(p.x(), p.y(), l1p0.x(), l1p0.y(), l1p1.x(), l1p1.y(), &x, &y);

	sNorm = dist0 / MAX(SMALL_NUMBER, dist0 + dist1);

	if (d0 > 0 && d1 < 0)
	{
		return true;
	}
	else if (d0 < 0 && d1 > 0)
	{
		sNorm = -sNorm;
		return true;
	}
	else
	{
		if (dist0 < dist1)
		{
			sNorm = -dist0;
		}
		else
		{
			sNorm = dist1;
		}

		return false;
	}
}

double GetLengthOfLine2D(double x1, double y1, double x2, double y2)
{
	return sqrt((x2 - x1)*(x2 - x1) + (y2 - y1)*(y2 - y1));
}

double GetLengthOfVector2D(double x, double y)
{
	return sqrt(x * x + y * y);
}

double GetLengthOfVector3D(double x, double y, double z)
{
	return sqrt(x*x + y*y + z*z);
}

void RotateVec2D(double x, double y, double angle, double &xr, double &yr)
{
	xr = x * cos(angle) - y * sin(angle);
	yr = x * sin(angle) + y * cos(angle);
}

void Global2LocalCoordinates(double xTargetGlobal, double yTargetGlobal,
							 double xHostGlobal, double yHostGlobal, double angleHost,
							 double &targetXforHost, double &targetYforHost)
{
	double relativeX = xTargetGlobal - xHostGlobal;
	double relativeY = yTargetGlobal - yHostGlobal;
	targetXforHost = relativeX * cos(angleHost) - relativeY * sin(angleHost);
	targetYforHost = relativeX * sin(angleHost) + relativeY * cos(angleHost);
}

void Local2GlobalCoordinates(double &xTargetGlobal, double &yTargetGlobal,
							 double xHostGlobal, double yHostGlobal, double thetaGlobal,
							 double targetXforHost, double targetYforHost)
{
  xTargetGlobal = targetXforHost * cos(-thetaGlobal) + targetYforHost *
    sin(-thetaGlobal) + xHostGlobal;

  yTargetGlobal = targetYforHost * cos(-thetaGlobal) - targetXforHost *
    sin(-thetaGlobal) + yHostGlobal;
}

void SwapByteOrder(unsigned char *buf, int data_type_size, int buf_size)
{
	unsigned char *ptr = buf;
	unsigned char tmp;

	if (data_type_size < 2)
	{
		// No need to swap for one byte data types
		return;
	}

	for (int i = 0; i < buf_size / data_type_size; i++)
	{
		for (int j = 0; j < data_type_size / 2; j++)
		{
			tmp = ptr[j];
			ptr[j] = ptr[data_type_size - j - 1];
			ptr[data_type_size - j - 1] = tmp;
		}
		ptr += data_type_size;
	}
}

int strtoi(std::string s)
{
	return atoi(s.c_str());
}

double strtod(std::string s)
{
	return atof(s.c_str());
}

#if (defined WINVER && WINVER == _WIN32_WINNT_WIN7 || __MINGW32__)

	#include <windows.h>
	#include <process.h>

	__int64 SE_getSystemTime()
	{
		return timeGetTime();
	}

	void SE_sleep(unsigned int msec)
	{
		Sleep(msec);
	}

#else

	#include <chrono>

	using namespace std::chrono;

	__int64 SE_getSystemTime()
	{
		return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	}

	void SE_sleep(unsigned int msec)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(msec)));
	}

#endif

double SE_getSimTimeStep(__int64 &time_stamp, double min_time_step, double max_time_step)
{
	double dt;

	__int64 now = SE_getSystemTime();

	if (time_stamp == 0)
	{
		// First call. Return minimal dt
		dt = min_time_step;
	}
	else
	{
		dt = static_cast<double>(now - time_stamp) * 0.001;  // step size in seconds

		if (dt > max_time_step) // limit step size
		{
			dt = max_time_step;
		}
		else if (dt < min_time_step)  // avoid CPU rush, sleep for a while
		{
			SE_sleep(static_cast<unsigned int>(static_cast<int>(min_time_step - dt) * 1000));
			now = SE_getSystemTime();
			dt = static_cast<double>(now - time_stamp) * 0.001;
		}
	}
	time_stamp = now;

	return dt;
}

std::vector<std::string> SplitString(const std::string& s, char separator)
{
	std::vector<std::string> output;
	std::string::size_type prev_pos = 0, pos = 0;

	while ((pos = s.find(separator, pos)) != std::string::npos)
	{
		std::string substring(s.substr(prev_pos, pos - prev_pos));
		output.push_back(substring);
		prev_pos = ++pos;
	}
	output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word

	return output;
}

std::string DirNameOf(const std::string& fname)
{
	size_t pos = fname.find_last_of("\\/");

	return (std::string::npos == pos) ? "./" : fname.substr(0, pos);
}

std::string FileNameOf(const std::string& fname)
{
	size_t pos = fname.find_last_of("\\/");
	if (pos != std::string::npos)
	{
		return (fname.substr(pos+1));
	}
	else
	{
		return fname;  // Assume filename with no separator
	}
}

bool IsDirectoryName(const std::string& string)
{
	if (!string.empty() && (string.back() == '/' || string.back() == '\\'))
	{
		return true;
	}

	return false;
}

std::string FileNameExtOf(const std::string& fname)
{
	size_t start_pos = fname.find_last_of("\\/");
	if (start_pos != std::string::npos)
	{
		start_pos++;
	}
	else
	{
		start_pos = 0;
	}

	size_t end_pos = fname.find_last_of(".");
	if (end_pos != std::string::npos)
	{
		return (fname.substr(end_pos, fname.length() - end_pos));
	}
	else
	{
		return ("");
	}
}

std::string FileNameWithoutExtOf(const std::string& fname)
{
	size_t start_pos = fname.find_last_of("\\/");
	if (start_pos != std::string::npos)
	{
		start_pos++;
	}
	else
	{
		start_pos = 0;
	}

	size_t end_pos = fname.find_last_of(".");
	if (end_pos != std::string::npos)
	{
		return (fname.substr(start_pos, end_pos - start_pos));
	}
	else
	{
		return (fname.substr(start_pos));
	}
}

std::string ToLower(const std::string in_str)
{
	std::locale loc;
	std::string out_str = in_str;

	for (size_t i = 0; i < out_str.size(); i++)
	{
		out_str[i] = std::tolower(out_str[i], loc);
	}

	return out_str;
}

std::string ToLower(const char* in_str)
{
	std::locale loc;
	std::string out_str = in_str;

	for (size_t i = 0; i < out_str.size(); i++)
	{
		out_str[i] = std::tolower(out_str[i], loc);
	}

	return out_str;
}

double GetCrossProduct2D(double x1, double y1, double x2, double y2)
{
	return x1 * y2 - x2 * y1;
}

double GetDotProduct2D(double x1, double y1, double x2, double y2)
{
	return x1 * x2 + y1 * y2;
}

void NormalizeVec2D(double x, double y, double &xn, double &yn)
{
	double len = sqrt(x*x + y*y);
	if (len < SMALL_NUMBER)
	{
		len = SMALL_NUMBER;
	}
	xn = x / len;
	yn = y / len;
}

void OffsetVec2D(double x0, double y0, double x1, double y1, double offset, double& xo0, double& yo0, double& xo1, double& yo1)
{
	double angle_line = atan2(y1 - y0, x1 - x0);
	double angle_offset = angle_line + (offset < 0 ? M_PI_2 : -M_PI_2);  // perpendicular to line
	double line_offset[2] = { fabs(offset) * cos(angle_offset), fabs(offset) * sin(angle_offset) };

	xo0 = x0 + line_offset[0];
	yo0 = y0 + line_offset[1];
	xo1 = x1 + line_offset[0];
	yo1 = y1 + line_offset[1];
}

void ZYZ2EulerAngles(double z0, double y, double z1, double& h, double& p, double& r)
{
	double cx = cos(z0);
	double cy = cos(y);
	double cz = cos(z1);
	double sx = sin(z0);
	double sy = sin(y);
	double sz = sin(z1);

	// Create a rotation matrix Z0 * Y * Z1
	double m[3][3] =
	{
		{cx * cy * cz - sx * sz, -cx * cy * sz - sx * cz, cx * sy} ,
		{sx * cy * cz + cx * sz, cx * cz - sx * cy * sz, sx * sy},
		{-sy * cz, sy * sz, cy}
	};

	// Avoid gimbal lock
	if (fabs(m[0][0]) < SMALL_NUMBER) m[0][0] = SIGN(m[0][0]) * SMALL_NUMBER;
	if (fabs(m[2][2]) < SMALL_NUMBER) m[2][2] = SIGN(m[2][2]) * SMALL_NUMBER;

	h = atan2(m[1][0], m[0][0]);
	p = atan2(-m[2][0], sqrt(m[2][1] * m[2][1] + m[2][2] * m[2][2]));
	r = atan2(m[2][1], m[2][2]);
}

void R0R12EulerAngles(double h0, double p0, double r0, double h1, double p1, double r1, double& h, double& p, double& r)
{
	// 1. Create two rotation matrices
	// 2. Multiply them
	// 3. Extract yaw. pitch , roll

	double cx = cos(h0);
	double cy = cos(p0);
	double cz = cos(r0);
	double sx = sin(h0);
	double sy = sin(p0);
	double sz = sin(r0);

	double R0[3][3] =
	{
		{cx * cy, cx * sy * sz - sx * cz, sx * sz + cx * sy * cz} ,
		{sx * cy, cx * cz + sx * sy * sz, sx * sy * cz - cx * sz},
		{-sy, cy * sz, cy * cz}
	};

	cx = cos(h1);
	cy = cos(p1);
	cz = cos(r1);
	sx = sin(h1);
	sy = sin(p1);
	sz = sin(r1);

	double R1[3][3] =
	{
		{cx * cy, cx * sy * sz - sx * cz, sx * sz + cx * sy * cz},
		{sx * cy, cx * cz + sx * sy * sz, sx * sy * cz - cx * sz},
		{-sy, cy * sz, cy * cz}
	};

	// Multiply
	double R2[3][3] = { {0, 0, 0}, {0, 0, 0}, {0, 0, 0} };
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			for (int k = 0; k < 3; k++)
				R2[i][j] += R0[i][k] * R1[k][j];

	// Avoid gimbal lock
	if (fabs(R2[0][0]) < SMALL_NUMBER) R2[0][0] = SIGN(R2[0][0]) * SMALL_NUMBER;
	if (fabs(R2[2][2]) < SMALL_NUMBER) R2[2][2] = SIGN(R2[2][2]) * SMALL_NUMBER;

	h = GetAngleInInterval2PI(atan2(R2[1][0], R2[0][0]));
	p = GetAngleInInterval2PI(atan2(-R2[2][0], sqrt(R2[2][1] * R2[2][1] + R2[2][2] * R2[2][2])));
	r = GetAngleInInterval2PI(atan2(R2[2][1], R2[2][2]));
}

SE_Env::SE_Env() : osiMaxLongitudinalDistance_(OSI_MAX_LONGITUDINAL_DISTANCE), osiMaxLateralDeviation_(OSI_MAX_LATERAL_DEVIATION),
	logFilePath_(LOG_FILENAME), datFilePath_(""), offScreenRendering_(true), collisionDetection_(false)
{
	seed_ = (std::random_device())();
	gen_.seed(seed_);
}

int SE_Env::AddPath(std::string path)
{
	// Check if path already in list
	for (size_t i = 0; i < paths_.size(); i++)
	{
		if (paths_[i] == path)
		{
			return -1;
		}
	}
	paths_.push_back(path);

	return 0;
}

std::string SE_Env::GetModelFilenameById(int model_id)
{
	std::string name;
	if (entity_model_map.size() == 0)
	{
		entity_model_map = ParseModelIds();
	}

	name = entity_model_map[model_id];

	if (name.empty())
	{
		LOG("Failed to lookup 3d model filename for model_id %d in list:", model_id);
		std::map<int, std::string>::iterator it;
		for (it = entity_model_map.begin(); it != entity_model_map.end(); ++it)
		{
			LOG("  %d %s", it->first, it->second.c_str());
		}
	}

	return name;
}

Logger::Logger() : callback_(0), time_(0)
{
	callback_ = 0;
	time_ = 0;
}

Logger::~Logger()
{
	if (file_.is_open())
	{
		file_.close();
	}

	callback_ = 0;
}

bool Logger::IsCallbackSet()
{
	return callback_ != 0;
}

void Logger::Log(bool quit, bool trace, char const* file, char const* func, int line, char const* format, ...)
{
	static char complete_entry[2048];
	static char message[1024];

	mutex_.Lock();  // Protect from simultanous use from different threads

	va_list args;
	va_start(args, format);
	vsnprintf(message, 1024, format, args);

#ifdef DEBUG_TRACE
	// enforce trace
	trace = true;
#endif
	if (time_)
	{
		if (trace)
		{
			snprintf(complete_entry, 2048, "%.3f %s / %d / %s(): %s", *time_, file, line, func, message);
		}
		else
		{
			snprintf(complete_entry, 2048, "%.3f: %s", *time_, message);
		}
	}
	else
	{
		if (trace)
		{
			snprintf(complete_entry, 2048, "%s / %d / %s(): %s", file, line, func, message);
		}
		else
		{
			strncpy(complete_entry, message, 1024);
		}
	}

	if (file_.is_open())
	{
		file_ << complete_entry << std::endl;
		file_.flush();
	}

	if (callback_)
	{
		callback_(complete_entry);
	}

	va_end(args);

	mutex_.Unlock();

	if (quit)
	{
		throw std::runtime_error(complete_entry);
	}
}

void Logger::SetCallback(FuncPtr callback)
{
	callback_ = callback;
}

Logger& Logger::Inst()
{
	static Logger instance_;
	return instance_;
}

void Logger::OpenLogfile(std::string filename)
{
#ifndef SUPPRESS_LOG
	if (!filename.empty())
	{
		if (file_.is_open())
		{
			// Close any open logfile, perhaps user want a new with unique filename
			file_.close();
		}

		file_.open(filename.c_str());
		if (file_.fail())
		{
			const char* filename_tmp = std::tmpnam(NULL);
			printf("Cannot open log file: %s in working directory. Trying system tmp-file: %s\n",
				SE_Env::Inst().GetLogFilePath().c_str(), filename_tmp);
			file_.open(filename_tmp);
			if (file_.fail())
			{
				printf("Also failed to open log file: %s. Continue without logfile, still logging to console.\n", filename_tmp);
			}
		}
	}
#endif
}

void Logger::LogVersion()
{
	static char message[1024];

	snprintf(message, 1024, "esmini GIT REV: %s", esmini_git_rev());
	if (file_.is_open()) file_ << message << std::endl;
	if (callback_) callback_(message);

	snprintf(message, 1024, "esmini GIT TAG: %s", esmini_git_tag());
	if (file_.is_open()) file_ << message << std::endl;
	if (callback_) callback_(message);

	snprintf(message, 1024, "esmini GIT BRANCH: %s", esmini_git_branch());
	if (file_.is_open()) file_ << message << std::endl;
	if (callback_) callback_(message);

	snprintf(message, 1024, "esmini BUILD VERSION: %s", esmini_build_version());
	if (file_.is_open()) file_ << message << std::endl;
	if (callback_) callback_(message);
}

SE_Env& SE_Env::Inst()
{
	static SE_Env instance_;
	return instance_;
}

void SE_Env::SetLogFilePath(std::string logFilePath)
{
	logFilePath_ = logFilePath;
	if (Logger::Inst().IsFileOpen())
	{
		// Probably user wants another logfile with a new name
		Logger::Inst().OpenLogfile(SE_Env::Inst().GetLogFilePath());
	}
}

void SE_Env::SetDatFilePath(std::string datFilePath)
{
	datFilePath_ = datFilePath;
	if (Logger::Inst().IsFileOpen())
	{
		// Probably user wants another logfile with a new name
		Logger::Inst().OpenLogfile(SE_Env::Inst().GetLogFilePath());
	}
}

/*
 * Logger for all vehicles contained in the Entities vector.
 *
 * Builds a header based on the number of vehicles then prints data
 * in columnar format, with time running from top to bottom and
 * vehicles running from left to right, starting with the Ego vehicle
 */
CSV_Logger::CSV_Logger() : data_index_(0), callback_(nullptr)
{

}

CSV_Logger::~CSV_Logger()
{
	if (file_.is_open())
	{
		file_.close();
	}

	callback_ = 0;
}

void CSV_Logger::LogVehicleData(bool isendline, double timestamp, char const* name, int id, double speed,
	double wheel_angle, double wheel_rot, double posX, double posY, double posZ, double velX, double velY,
	double velZ, double accX, double accY, double accZ, double distance_road, double distance_lanem, double heading,
	double heading_rate, double heading_angle, double heading_angle_driving_direction, double pitch, double curvature,
	const char* collisions, ...)
{
	static char data_entry[max_csv_entry_length];

	//If this data is for Ego (position 0 in the Entities vector) print using the first format
	//Otherwise use the second format
	if (id == 0)
		snprintf(data_entry, max_csv_entry_length,
			"%d, %f, %s, %d, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %s, ",
			data_index_, timestamp, name, id, speed, wheel_angle, wheel_rot, posX, posY, posZ, velX,
			velY, velZ, accX, accY, accZ, distance_road, distance_lanem, heading, heading_rate,
			heading_angle, heading_angle_driving_direction, pitch, curvature, collisions);
	else
		snprintf(data_entry, max_csv_entry_length,
			"%s, %d, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %s, ",
			name, id, speed, wheel_angle, wheel_rot, posX, posY, posZ, velX, velY, velZ, accX, accY, accZ,
			distance_road, distance_lanem, heading, heading_rate, heading_angle, heading_angle_driving_direction,
			pitch, curvature, collisions);

	//Add lines horizontally until the endline is reached
	if (isendline == false)
	{
		file_ << data_entry;
	}
	else if (file_.is_open())
	{

		file_ << data_entry << std::endl;
		file_.flush();

		data_index_++;
	}


	if (callback_)
	{
		callback_(data_entry);
	}
}

void CSV_Logger::SetCallback(FuncPtr callback)
{
	callback_ = callback;

	static char message[1024];

	snprintf(message, 1024, "esmini GIT REV: %s", esmini_git_rev());
	callback_(message);
	snprintf(message, 1024, "esmini GIT TAG: %s", esmini_git_tag());
	callback_(message);
	snprintf(message, 1024, "esmini GIT BRANCH: %s", esmini_git_branch());
	callback_(message);
	snprintf(message, 1024, "esmini BUILD VERSION: %s", esmini_build_version());
	callback_(message);
}

//instantiator
//Filename and vehicle number are used for dynamic header creation
void CSV_Logger::Open(std::string scenario_filename, int numvehicles, std::string csv_filename)
{
	if (file_.is_open())
	{
		file_.close();
	}

	file_.open(csv_filename);
	if (file_.fail())
	{
		throw std::iostream::failure(std::string("Cannot open file: ") + csv_filename);
	}

	data_index_ = 0;

	//Standard ESMINI log header, appended with Scenario file name and vehicle count
	static char message[max_csv_entry_length];
	snprintf(message, max_csv_entry_length, "esmini GIT REV: %s", esmini_git_rev());
	file_ << message << std::endl;
	snprintf(message, max_csv_entry_length, "esmini GIT TAG: %s", esmini_git_tag());
	file_ << message << std::endl;
	snprintf(message, max_csv_entry_length, "esmini GIT BRANCH: %s", esmini_git_branch());
	file_ << message << std::endl;
	snprintf(message, max_csv_entry_length, "esmini BUILD VERSION: %s", esmini_build_version());
	file_ << message << std::endl;
	snprintf(message, max_csv_entry_length, "Scenario File Name: %s", scenario_filename.c_str());
	file_ << message << std::endl;
	snprintf(message, max_csv_entry_length, "Number of Vehicles: %d", numvehicles);
	file_ << message << std::endl;

	//Ego vehicle is always present, at least one set of vehicle data values should be stored
	//Index and TimeStamp are included in this first set of columns
	snprintf(message, max_csv_entry_length,
		"Index [-] , TimeStamp [s] , #1 Entitity_Name [-] , "
		"#1 Entitity_ID [-] , #1 Current_Speed [m/s] , #1 Wheel_Angle [deg] , "
		"#1 Wheel_Rotation [-] , #1 World_Position_X [m] , #1 World_Position_Y [m] , "
		"#1 World_Position_Z [m] , #1 Vel_X [m/s] , #1 Vel_Y [m/s] , #1 Vel_Z [m/s] , "
		"#1 Acc_X [m/s2] , #1 Acc_Y [m/s2] , #1 Acc_Z [m/s2] , #1 Distance_Travelled_Along_Road_Segment [m] , "
		"#1 Lateral_Distance_Lanem [m] , #1 World_Heading_Angle [rad] , #1 Heading_Angle_Rate [rad/s] , "
		"#1 Relative_Heading_Angle [rad] , #1 Relative_Heading_Angle_Drive_Direction [rad] , "
		"#1 World_Pitch_Angle [rad] , #1 Road_Curvature [1/m] , #1 collision_ids , ");
	file_ << message;

	//Based on number of vehicels in the Entities vector, extend the header accordingly
	for (int i = 2; i <= numvehicles; i++)
	{
		snprintf(message, max_csv_entry_length,
			"#%d Entitity_Name [-] , #%d Entitity_ID [-] , "
			"#%d Current_Speed [m/s] , #%d Wheel_Angle [deg] , #%d Wheel_Rotation [-] , "
			"#%d World_Position_X [m] , #%d World_Position_Y [m] , #%d World_Position_Z [m] , "
			"#%d Vel_X [m/s] , #%d Vel_Y [m/s] , #%d Vel_Z [m/s] , #%d Acc_X [m/s2] , #%d Acc_Y [m/s2] , #%d Acc_Z [m/s2] , "
			"#%d Distance_Travelled_Along_Road_Segment [m] , #%d Lateral_Distance_Lanem [m] , "
			"#%d World_Heading_Angle [rad] , #%d Heading_Angle_Rate [rad/s] , #%d Relative_Heading_Angle [rad] , "
			"#%d Relative_Heading_Angle_Drive_Direction [rad] , #%d World_Pitch_Angle [rad] , "
			"#%d Road_Curvature [1/m] , #%d collision_ids , "
			, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i);
		file_ << message;
	}
	file_ << std::endl;

	file_.flush();

	callback_ = 0;
}

CSV_Logger& CSV_Logger::Inst()
{
	static CSV_Logger instance_;
	return instance_;
}

SE_Thread::~SE_Thread()
{
	Wait();
}

void SE_Thread::Start(void(*func_ptr)(void*), void *arg)
{
#if (defined WINVER && WINVER == _WIN32_WINNT_WIN7 || __MINGW32__)
	thread_ = (void*)_beginthread(func_ptr, 0, arg);
#else
	thread_ = std::thread(func_ptr, arg);
#endif
}


void SE_Thread::Wait()
{
#if (defined WINVER && WINVER == _WIN32_WINNT_WIN7 || __MINGW32__)
	WaitForSingleObject((HANDLE)thread_, 3000);  // Should never need to wait for more than 3 sec
#else
	if (thread_.joinable())
	{
		thread_.join();
	}
#endif
}

SE_Mutex::SE_Mutex()
{
#if (defined WINVER && WINVER == _WIN32_WINNT_WIN7 || MINGW32)
	mutex_ = (void*)CreateMutex(
		NULL,              // default security attributes
		0,             // initially not owned
		NULL);             // unnamed mutex

	if (mutex_ == NULL)
	{
		LOG("CreateMutex error: %d\n", GetLastError());
		mutex_ = 0;
	}
#else

#endif
}


void SE_Mutex::Lock()
{
#if (defined WINVER && WINVER == _WIN32_WINNT_WIN7 || __MINGW32__)
	WaitForSingleObject(mutex_, 1000);  // Should never need to wait for more than 1 sec
#else
	mutex_.lock();
#endif
}

void SE_Mutex::Unlock()
{
#if (defined WINVER && WINVER == _WIN32_WINNT_WIN7 || __MINGW32__)
	ReleaseMutex(mutex_);
#else
	mutex_.unlock();
#endif
}


void SE_Option::Usage()
{
	if (!default_value_.empty())
	{
		printf("  %s%s %s", OPT_PREFIX, opt_str_.c_str(), (opt_arg_ != "") ? '[' + opt_arg_.c_str() + ']' : "");
	}
	else
	{
		printf("  %s%s %s", OPT_PREFIX, opt_str_.c_str(), (opt_arg_ != "") ? '<' + opt_arg_.c_str() + '>' : "");
	}

	if (!default_value_.empty())
	{
		printf("  (default = %s)", default_value_.c_str());
	}
	printf("\n      %s\n", opt_desc_.c_str());
}


void SE_Options::AddOption(std::string opt_str, std::string opt_desc, std::string opt_arg)
{
	SE_Option opt(opt_str, opt_desc, opt_arg);
	option_.push_back(opt);
}

void SE_Options::AddOption(std::string opt_str, std::string opt_desc, std::string opt_arg, std::string default_value)
{
	SE_Option opt(opt_str, opt_desc, opt_arg, default_value);
	option_.push_back(opt);
}

void SE_Options::PrintUsage()
{
	printf("\nUsage: %s [options]\n", app_name_.c_str());
	printf("Options: \n");
	for (size_t i = 0; i < option_.size(); i++)
	{
		option_[i].Usage();
	}
	printf("\n");
}

void SE_Options::PrintUnknownArgs(std::string message)
{
	printf("\n%s\n", message.c_str());
	for (const auto& arg : unknown_args_)
	{
		printf("  %s\n", arg.c_str());
	}
}

bool SE_Options::GetOptionSet(std::string opt)
{
	SE_Option *option = GetOption(opt);

	if (option)
	{
		return option->set_;
	}
	else
	{
		return false;
	}
}

bool SE_Options::IsOptionArgumentSet(std::string opt)
{
	return GetOption(opt)->set_;
}

std::string SE_Options::GetOptionArg(std::string opt, int index)
{
	SE_Option *option = GetOption(opt);

	if (option == nullptr)
	{
		return "";
	}

	if (!(option->opt_arg_.empty()) && static_cast<unsigned int>(index) < option->arg_value_.size())
	{
		return option->arg_value_[static_cast<unsigned int>(index)];
	}
	else
	{
		return "";
	}
}

static constexpr std::array<const char*, 10> OSG_ARGS = {
	"--clear-color",
	"--screen",
	"--window",
	"--borderless-window",
	"--SingleThreaded",
	"--CullDrawThreadPerContext",
	"--SingleThreaded",
	"--DrawThreadPerContext",
	"--CullThreadPerCameraDrawThreadPerContext",
	"--lodScale"
};

int SE_Options::ChangeOptionArg(std::string opt, std::string new_value, int index)
{
	SE_Option* option = GetOption(opt);

	if (option == nullptr || index < 0 || static_cast<unsigned int>(index) >= option->arg_value_.size())
	{
		return -1;
	}

	option->arg_value_[static_cast<unsigned int>(index)] = new_value;

	return 0;
}

int SE_Options::ParseArgs(int argc, const char* const argv[])
{
	std::vector<const char*> args = {argv, std::next(argv, argc)};


	app_name_ = FileNameWithoutExtOf(args[0]);
	int returnVal = 0;

	for (size_t i = 0; i < static_cast<unsigned int>(argc); i++)
	{
		originalArgs_.push_back(args[i]);
	}

	for (size_t i = 1; i < static_cast<unsigned int>(argc);)
	{
		std::string arg = args[i];

		if (!(arg.substr(0, strlen(OPT_PREFIX)) == OPT_PREFIX))
		{
			i++;
			continue;
		}

		SE_Option *option = GetOption(&args[i][strlen(OPT_PREFIX)]); // skip prefix

		if (option)
		{
			option->set_ = true;
			if (option->opt_arg_ != "")
			{
				if (i < static_cast<unsigned int>(argc - 1) && strncmp(args[i + 1], "--", 2))
				{
					option->arg_value_.push_back(args[i+1]);
					i++;
				}
				else if (!option->default_value_.empty())
				{
					option->arg_value_.push_back(option->default_value_);
				}
				else
				{
					LOG("Argument parser error: Missing option %s argument", option->opt_str_.c_str());
					option->set_ = false;
					returnVal = -1;
				}
			}
		}
		else
		{
			auto it = std::find_if(std::begin(OSG_ARGS), std::end(OSG_ARGS), [&arg](const char* osg_arg) {
			  return osg_arg == arg;
			});
			if (it == std::end(OSG_ARGS)) {
				unknown_args_.push_back(args[i]);
			}
		}
		i++;
	}

	return returnVal;
}

SE_Option* SE_Options::GetOption(std::string opt)
{
	for (size_t i = 0; i < option_.size(); i++)
	{
		if (opt == option_[i].opt_str_)
		{
			return &option_[i];
		}
	}
	return 0;
}

bool SE_Options::IsInOriginalArgs(std::string opt)
{
	if (std::find(originalArgs_.begin(), originalArgs_.end(), opt) != originalArgs_.end())
	{
		return true;
	}

	return false;
}

bool SE_Options::HasUnknownArgs()
{
	return !unknown_args_.empty();
}

void SE_Options::Reset()
{
	for (size_t i = 0; i < option_.size(); i++)
	{
		option_[i].arg_value_.clear();
	}
	option_.clear();
	originalArgs_.clear();
}

int SE_WritePPM(const char* filename, int width, int height, const unsigned char* data, int pixelSize, int pixelFormat, bool upsidedown)
{
	FILE* file;

	if ((file = fopen(filename, "wb")) == nullptr)
	{
		return -1;
	}

	if (pixelSize != 3)
	{
		LOG("PPM PixelSize %d not supported yet, only 3", pixelSize);
		return -2;
	}

	if (pixelFormat != static_cast<int>(PixelFormat::BGR) && pixelFormat != static_cast<int>(PixelFormat::RGB))
	{
		LOG("PPM PixelFormat %d not supported yet, only 0x%x (RGB) and 0x%x (BGR)", PixelFormat::RGB, PixelFormat::BGR);
		return -3;
	}

	/* Write PPM Header */
	fprintf(file, "P6 %d %d %d\n", width, height, 255); /* width, height, max color value */

	/* Write Image Data */
	if (pixelFormat == static_cast<int>(PixelFormat::RGB))
	{
		if (upsidedown)
		{
			for (int i = 0; i < height; i++)
			{
				for (int j = 0; j < width; j++)
				{
					// write one line at a time, starting from bottom
					fwrite(&data[pixelSize * ((height - i - 1) * width + j)], 3, 1, file);
				}
			}
		}
		else
		{
			// Write all RBG pixel values as a whole chunk
			fwrite(data, 3, static_cast<unsigned int>(width * height), file);
		}
	}
	else
	{
		if (upsidedown)
		{
			for (int i = 0; i < height; i++)
			{
				for (int j = 0; j < width; j++)
				{
					// write one line at a time, starting from bottom
					const unsigned char* ptr = &data[pixelSize * ((height - i - 1) * width + j)];
					unsigned char bytes[3] = { (ptr[2]), ptr[1], ptr[0] };
					fwrite(bytes, 3, 1, file);
				}
			}
		}
		else
		{
			for (int i = 0; i < width * height; i++)
			{
				const unsigned char* ptr = &data[i * pixelSize];
				unsigned char bytes[3] = { (ptr[2]), ptr[1], ptr[0] };
				fwrite(bytes, 3, 1, file);
			}
		}
	}

	fclose(file);

	return 0;
}

int SE_WriteTGA(const char* filename, int width, int height, const unsigned char* data, int pixelSize, int pixelFormat, bool upsidedown)
{
	FILE* file;

	if ((file = fopen(filename, "wb")) == nullptr)
	{
		return -1;
	}

	if (pixelSize != 3)
	{
		LOG("TGA PixelSize %d not supported yet, only 3", pixelSize);
		return -2;
	}

	if (pixelFormat != static_cast<int>(PixelFormat::BGR) && pixelFormat != static_cast<int>(PixelFormat::RGB))
	{
		LOG("TGA PixelFormat 0x%x not supported yet, only 0x%x (RGB) and 0x%x (BGR)", pixelFormat, PixelFormat::RGB, PixelFormat::BGR);
		return -3;
	}

	/* Write TGA Header */
	uint8_t header[18] = {
		0,
		0,
		2,						  // uncompressed RGB
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		static_cast<uint8_t>(width & 0x00FF),
		static_cast<uint8_t>((width & 0xFF00) >> 8),
		static_cast<uint8_t>(height & 0x00FF),
		static_cast<uint8_t>((height & 0xFF00) >> 8),
		static_cast<uint8_t>(pixelSize * 8),  // 24 (BGR)
		static_cast<uint8_t>(upsidedown ? 0 : (1<<5))    // bit 5 (6:th) controls vertical direction
	};
	fwrite(&header, 18, 1, file);

	/* Write Image Data */
	if (pixelFormat == static_cast<int>(PixelFormat::RGB))
	{
		for (int i = 0; i < width * height; i++)
		{
			const unsigned char* ptr = &data[i * pixelSize];
			unsigned char bytes[3] = { (ptr[2]), ptr[1], ptr[0] };
			fwrite(bytes, 3, 1, file);
		}
	}
	else
	{
		fwrite(data, static_cast<unsigned int>(width * height * pixelSize), 1, file);
	}

	fclose(file);

	return 0;
}

int SE_ReadCSVFile(const char* filename, std::vector<std::vector<std::string>>& content, int skip_lines)
{
	// Cred: https://java2blog.com/read-csv-file-in-cpp/
	std::vector<std::string> row;
	std::string line, word;

	std::fstream file(filename, std::ios::in);
	if (file.is_open())
	{
		for (int i = 0; i < skip_lines; i++)
		{
			if (!getline(file, line))
			{
				LOG("Failed to skip %d lines in CSV file %s", skip_lines, filename);
				return -1;
			}
		}
		while (getline(file, line))
		{
			row.clear();

			std::stringstream str(line);

			while (getline(str, word, ','))
				row.push_back(word);
			content.push_back(row);
		}
	}
	else
	{
		LOG("Failed to open CSV file %s", filename);
		return -1;
	}

	return 0;
}
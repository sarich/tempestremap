///////////////////////////////////////////////////////////////////////////////
///
///	\file    GenerateOverlapMesh.cpp
///	\author  Paul Ullrich
///	\version March 7, 2014
///
///	<remarks>
///		Copyright 2000-2014 Paul Ullrich
///
///		This file is distributed as part of the Tempest source code package.
///		Permission is granted to use, copy, modify and distribute this
///		source code and its documentation under the terms of the GNU General
///		Public License.  This software is provided "as is" without express
///		or implied warranty.
///	</remarks>

#include "Announce.h"
#include "CommandLine.h"
#include "Exception.h"
#include "GridElements.h"

#include "TempestRemapAPI.h"

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {

	// Input mesh A
	std::string strMeshA;

	// Input mesh B
	std::string strMeshB;

	// Output mesh file
	std::string strOverlapMesh;

	// Overlap grid generation method
	std::string strMethod;

	// No validation of the meshes
	bool fNoValidate;

	// Parse the command line
	BeginCommandLine()
		CommandLineString(strMeshA, "a", "");
		CommandLineString(strMeshB, "b", "");
		CommandLineString(strOverlapMesh, "out", "overlap.g");
		CommandLineStringD(strMethod, "method", "fuzzy", "(fuzzy|exact|mixed)");
		CommandLineBool(fNoValidate, "novalidate");

		ParseCommandLine(argc, argv);
	EndCommandLine(argv)

	AnnounceBanner();

	// Call the actual mesh generator
	Mesh meshOverlap;
	int err = GenerateOverlapMesh_v1(strMeshA, strMeshB, meshOverlap, strOverlapMesh, strMethod, fNoValidate);

	AnnounceBanner();

	if (err) exit(err);
	else return 0;
}

///////////////////////////////////////////////////////////////////////////////

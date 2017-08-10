///////////////////////////////////////////////////////////////////////////////
///
///	\file    GenerateRLLMesh.cpp
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

#include "CommandLine.h"
#include "GridElements.h"
#include "Exception.h"
#include "Announce.h"

#include <cmath>
#include <iostream>

#include "netcdfcpp.h"

///////////////////////////////////////////////////////////////////////////////

#define ONLY_GREAT_CIRCLES

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {

	NcError error(NcError::silent_nonfatal);

try {
	// Number of longitudes in mesh
	int nLongitudes;

	// Number of latitudes in mesh
	int nLatitudes;

	// First longitude line on mesh
	double dLonBegin;

	// Last longitude line on mesh
	double dLonEnd;

	// First latitude line on mesh
	double dLatBegin;

	// Last latitude line on mesh
	double dLatEnd;

	// Flip latitude and longitude dimension in FaceVector ordering
	bool fFlipLatLon;

	// Input filename
	std::string strInputFile;

	// Input mesh is global
	bool fForceGlobal;

	// Verbose output
	bool fVerbose;

	// Output filename
	std::string strOutputFile;

	// Parse the command line
	BeginCommandLine()
		CommandLineInt(nLongitudes, "lon", 128);
		CommandLineInt(nLatitudes, "lat", 64);
		CommandLineDouble(dLonBegin, "lon_begin", 0.0);
		CommandLineDouble(dLonEnd, "lon_end", 360.0);
		CommandLineDouble(dLatBegin, "lat_begin", -90.0);
		CommandLineDouble(dLatEnd, "lat_end", 90.0);
		CommandLineBool(fFlipLatLon, "flip");
		CommandLineString(strInputFile, "in_file", "");
		CommandLineBool(fForceGlobal, "in_global");
		CommandLineBool(fVerbose, "verbose");
		CommandLineString(strOutputFile, "file", "outRLLMesh.g");

		ParseCommandLine(argc, argv);
	EndCommandLine(argv)

	// Verify latitude box is increasing
	if (dLatBegin >= dLatEnd) {
		_EXCEPTIONT("--lat_begin and --lat_end must specify a positive interval");
	}
	if (dLonBegin >= dLonEnd) {
		_EXCEPTIONT("--lon_begin and --lon_end must specify a positive interval");
	}

	std::cout << "=========================================================";
	std::cout << std::endl;

	// Longitude and latitude arrays
	DataVector<double> dLonEdge;
	DataVector<double> dLatEdge;

	// Generate mesh from input datafile
	if (strInputFile != "") {

		std::cout << "Generating mesh from input datafile ";
		std::cout << "\"" << strInputFile << "\"" << std::endl;

		NcFile ncfileInput(strInputFile.c_str(), NcFile::ReadOnly);
		if (!ncfileInput.is_valid()) {
			_EXCEPTION1("Unable to load input file \"%s\"", strInputFile.c_str());
		}

		NcDim * dimLon = ncfileInput.get_dim("lon");
		if (dimLon == NULL) {
			_EXCEPTIONT("Input file missing dimension \"lon\"");
		}

		NcDim * dimLat = ncfileInput.get_dim("lat");
		if (dimLat == NULL) {
			_EXCEPTIONT("Input file missing dimension \"lat\"");
		}

		NcVar * varLon = ncfileInput.get_var("lon");
		if (varLon == NULL) {
			_EXCEPTIONT("Input file missing variable \"lon\"");
		}

		NcVar * varLat = ncfileInput.get_var("lat");
		if (varLon == NULL) {
			_EXCEPTIONT("Input file missing variable \"lat\"");
		}

		nLongitudes = dimLon->size();
		nLatitudes = dimLat->size();

		if (nLongitudes < 2) {
			_EXCEPTIONT("At least two longitudes required in input file");
		}
		if (nLatitudes < 2) {
			_EXCEPTIONT("At least two latitudes required in input file");
		}

		DataVector<double> dLonNode(nLongitudes);
		varLon->set_cur((long)0);
		varLon->get(&(dLonNode[0]), nLongitudes);

		DataVector<double> dLatNode(nLatitudes);
		varLat->set_cur((long)0);
		varLat->get(&(dLatNode[0]), nLatitudes);

		for (int i = 0; i < nLongitudes-1; i++) {
			if (dLonNode[i] > dLonNode[i+1]) {
				_EXCEPTIONT("Longitudes must be monotone increasing");
			}
		}

		for (int j = 0; j < nLatitudes-1; j++) {
			if (dLatNode[j] > dLatNode[j+1]) {
				_EXCEPTIONT("Latitudes must be monotone increasing");
			}
		}

		double dFirstDeltaLon = dLonNode[1] - dLonNode[0];
		double dSecondLastDeltaLon = dLonNode[nLongitudes-1] - dLonNode[nLongitudes-2];
		double dLastDeltaLon = dLonNode[0] - (dLonNode[nLongitudes-1] - 360.0);

		if (fabs(dFirstDeltaLon - dLastDeltaLon) < 1.0e-12) {
			std::cout << "Mesh assumed periodic in longitude" << std::endl;
			fForceGlobal = true;
		}

		// Initialize longitude edges
		dLonEdge.Initialize(nLongitudes+1);

		if (fForceGlobal) {
			dLonEdge[0] = 0.5 * (dLonNode[0] + dLonNode[nLongitudes-1] - 360.0);
			dLonEdge[nLongitudes] = dLonEdge[0];
		} else {
			dLonEdge[0] = dLonNode[0] - 0.5 * dFirstDeltaLon;
			dLonEdge[nLongitudes] = dLonNode[nLongitudes-1] + 0.5 * dSecondLastDeltaLon;
		}

		for (int i = 1; i < nLongitudes; i++) {
			dLonEdge[i] = 0.5 * (dLonNode[i-1] + dLonNode[i]);
		}

		// Initialize latitude edges
		dLatEdge.Initialize(nLatitudes+1);

		dLatEdge[0] =
			dLatNode[0]
			- 0.5 * (dLatNode[1] - dLatNode[0]);

		if (dLatEdge[0] < -90.0) {
			dLatEdge[0] = -90.0;
		}

		dLatEdge[nLatitudes] =
			dLatNode[nLatitudes-1]
			+ 0.5 * (dLatNode[nLatitudes-1] - dLatNode[nLatitudes-2]);

		if (dLatEdge[nLatitudes] > 90.0) {
			dLatEdge[nLatitudes] = 90.0;
		}

		for (int j = 1; j < nLatitudes; j++) {
			dLatEdge[j] = 0.5 * (dLatNode[j-1] + dLatNode[j]);
		}

		// Convert all longitudes and latitudes to radians
		if (fVerbose) {
			std::cout << "Longitudes: ";
		}
		for (int i = 0; i <= nLongitudes; i++) {
			if (fVerbose) {
				std::cout << dLonEdge[i] << ", ";
			}
			dLonEdge[i] *= M_PI / 180.0;
		}
		if (fVerbose) {
			std::cout << std::endl;
			std::cout << "Latitudes: ";
		}
		for (int j = 0; j <= nLatitudes; j++) {
			if (fVerbose) {
				std::cout << dLatEdge[j] << ", ";
			}
			dLatEdge[j] *= M_PI / 180.0;
		}
		if (fVerbose) {
			std::cout << std::endl;
		}

	// Generate mesh from parameters
	} else {
		// Convert latitude and longitude interval to radians
		dLonBegin *= M_PI / 180.0;
		dLonEnd   *= M_PI / 180.0;
		dLatBegin *= M_PI / 180.0;
		dLatEnd   *= M_PI / 180.0;

		// Deltas in longitude and latitude directions
		double dDeltaLon = dLonEnd - dLonBegin;
		double dDeltaLat = dLatEnd - dLatBegin;

		// Create longitude arrays
		dLonEdge.Initialize(nLongitudes+1);
		for (int i = 0; i <= nLongitudes; i++) {
			double dLambdaFrac =
				  static_cast<double>(i)
				/ static_cast<double>(nLongitudes);

			dLonEdge[i] = dDeltaLon * dLambdaFrac + dLonBegin;
		}

		// Create latitude arrays
		dLatEdge.Initialize(nLatitudes+1);
		for (int j = 0; j <= nLatitudes; j++) {
			double dPhiFrac =
				  static_cast<double>(j)
				/ static_cast<double>(nLatitudes);

			dLatEdge[j] = dDeltaLat * dPhiFrac + dLatBegin;
		}
	}

	std::cout << "..Generating mesh with resolution [";
	std::cout << nLongitudes << ", " << nLatitudes << "]" << std::endl;
	std::cout << "..Longitudes in range [";
	std::cout << dLonBegin << ", " << dLonEnd << "]" << std::endl;
	std::cout << "..Latitudes in range [";
	std::cout << dLatBegin << ", " << dLatEnd << "]" << std::endl;
	std::cout << std::endl;

	// Verify arrays have been created successfully
	if (dLatEdge.GetRows() < 2) {
		_EXCEPTIONT("Invalid array of latitudes");
	}
	if (dLonEdge.GetRows() < 2) {
		_EXCEPTIONT("Invalid array of longitudes");
	}

	// Set bounds
	dLatBegin = dLatEdge[0];
	dLatEnd = dLatEdge[dLatEdge.GetRows()-1];
	dLonBegin = dLonEdge[0];
	dLonEnd = dLatEdge[dLonEdge.GetRows()-1];

	// Generate the mesh
	Mesh mesh;

	NodeVector & nodes = mesh.nodes;
	FaceVector & faces = mesh.faces;

	// Check if longitudes wrap
	bool fWrapLongitudes = false;
	if (fmod(dLonEnd - dLonBegin, 2.0 * M_PI) < 1.0e-12) {
		fWrapLongitudes = true;
	}
	bool fIncludeSouthPole = (fabs(dLatBegin + 0.5 * M_PI) < 1.0e-12);
	bool fIncludeNorthPole = (fabs(dLatEnd   - 0.5 * M_PI) < 1.0e-12);

	int iSouthPoleOffset = (fIncludeSouthPole)?(1):(0);

	// Increase number of latitudes if south pole is not included
	int iInteriorLatBegin = (fIncludeSouthPole)?(1):(0);
	int iInteriorLatEnd   = (fIncludeNorthPole)?(nLatitudes-1):(nLatitudes);

	// Number of longitude nodes
	int nLongitudeNodes = nLongitudes;
	if (!fWrapLongitudes) {
		nLongitudeNodes++;
	}

	// Generate nodes
	if (fIncludeSouthPole) {
		nodes.push_back(Node(0.0, 0.0, -1.0));
	}
	for (int j = iInteriorLatBegin; j < iInteriorLatEnd+1; j++) {
		for (int i = 0; i < nLongitudeNodes; i++) {

			double dLambda = dLonEdge[i];
			double dPhi = dLatEdge[j];

			double dX = cos(dPhi) * cos(dLambda);
			double dY = cos(dPhi) * sin(dLambda);
			double dZ = sin(dPhi);

			nodes.push_back(Node(dX, dY, dZ));
		}
	}
	if (fIncludeNorthPole) {
		nodes.push_back(Node(0.0, 0.0, +1.0));
	}

	// Generate south polar faces
	if (fIncludeSouthPole) {
		for (int i = 0; i < nLongitudes; i++) {
			Face face(4);
			face.SetNode(0, 0);
			face.SetNode(1, (i+1) % nLongitudeNodes + 1);
			face.SetNode(2, i + 1);
			face.SetNode(3, 0);

#ifndef ONLY_GREAT_CIRCLES
			face.edges[1].type = Edge::Type_ConstantLatitude;
			face.edges[3].type = Edge::Type_ConstantLatitude;
#endif

			faces.push_back(face);
		}
	}

	// Generate interior faces
	for (int j = iInteriorLatBegin; j < iInteriorLatEnd; j++) {
		int jx = j - iInteriorLatBegin;

		int iThisLatNodeIx =  jx    * nLongitudeNodes + iSouthPoleOffset;
		int iNextLatNodeIx = (jx+1) * nLongitudeNodes + iSouthPoleOffset;

		for (int i = 0; i < nLongitudes; i++) {
			Face face(4);
			face.SetNode(0, iThisLatNodeIx + (i + 1) % nLongitudeNodes);
			face.SetNode(1, iNextLatNodeIx + (i + 1) % nLongitudeNodes);
			face.SetNode(2, iNextLatNodeIx + i);
			face.SetNode(3, iThisLatNodeIx + i);

#ifndef ONLY_GREAT_CIRCLES
			face.edges[1].type = Edge::Type_ConstantLatitude;
			face.edges[3].type = Edge::Type_ConstantLatitude;
#endif

			faces.push_back(face);
		}
	}

	// Generate north polar faces
	if (fIncludeNorthPole) {
		int jx = nLatitudes - iInteriorLatBegin - 1;

		int iThisLatNodeIx =  jx    * nLongitudeNodes + iSouthPoleOffset;
		int iNextLatNodeIx = (jx+1) * nLongitudeNodes + iSouthPoleOffset;

		int iNorthPolarNodeIx = static_cast<int>(nodes.size()-1);
		for (int i = 0; i < nLongitudes; i++) {
			Face face(4);
			face.SetNode(0, iNorthPolarNodeIx);
			face.SetNode(1, iThisLatNodeIx + i);
			face.SetNode(2, iThisLatNodeIx + (i + 1) % nLongitudeNodes);
			face.SetNode(3, iNorthPolarNodeIx);

#ifndef ONLY_GREAT_CIRCLES
			face.edges[1].type = Edge::Type_ConstantLatitude;
			face.edges[3].type = Edge::Type_ConstantLatitude;
#endif

			faces.push_back(face);
		}
	}

	// Reorder the faces
	if (fFlipLatLon) {
		FaceVector faceTemp = mesh.faces;
		mesh.faces.clear();
		for (int i = 0; i < nLongitudes; i++) {
		for (int j = 0; j < nLatitudes; j++) {
			mesh.faces.push_back(faceTemp[j * nLongitudes + i]);
		}
		}
	}

	// Announce
	std::cout << "Writing mesh to file [" << strOutputFile.c_str() << "] ";
	std::cout << std::endl;

	// Output the mesh
	mesh.Write(strOutputFile);

	// Add rectilinear properties
	NcFile ncOutput(strOutputFile.c_str(), NcFile::Write);
	ncOutput.add_att("rectilinear", "true");

	if (fFlipLatLon) {
		ncOutput.add_att("rectilinear_dim0_size", nLongitudes);
		ncOutput.add_att("rectilinear_dim1_size", nLatitudes);
		ncOutput.add_att("rectilinear_dim0_name", "lon");
		ncOutput.add_att("rectilinear_dim1_name", "lat");
	} else {
		ncOutput.add_att("rectilinear_dim0_size", nLatitudes);
		ncOutput.add_att("rectilinear_dim1_size", nLongitudes);
		ncOutput.add_att("rectilinear_dim0_name", "lat");
		ncOutput.add_att("rectilinear_dim1_name", "lon");
	}
	ncOutput.close();

	// Announce
	std::cout << "..Mesh generator exited successfully" << std::endl;
	std::cout << "=========================================================";
	std::cout << std::endl;

	return (0);

} catch(Exception & e) {
	Announce(e.ToString().c_str());
	return (-1);

} catch(...) {
	return (-2);
}
}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
///
///	\file    GenerateTestData.cpp
///	\author  Paul Ullrich
///	\version August 31st, 2014
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
#include "FiniteElementTools.h"
#include "TriangularQuadrature.h"
#include "GaussQuadrature.h"
#include "GaussLobattoQuadrature.h"
#include "Exception.h"
#include "Announce.h"
#include "DataArray3D.h"

#include "netcdfcpp.h"

#include <cmath>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A class wrapper for a test function.
///	</summary>
class TestFunction {

public:
	///	<summary>
	///		Virtual destructor.
	///	</summary>
	virtual ~TestFunction()
	{ }

	///	<summary>
	///		Evaluate the test function.
	///	</summary>
	virtual double operator()(
		double dLon,
		double dLat
	) = 0;
};

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A relatively smooth low-order harmonic.
///	</summary>
class TestFunctionY2b2 : public TestFunction {

public:
	///	<summary>
	///		Evaluate the test function.
	///	<summary>
	virtual double operator()(
		double dLon,
		double dLat
	) {
		return (2.0 + cos(dLat) * cos(dLat) * cos(2.0 * dLon));
	}
};

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A high frequency spherical harmonic.
///	</summary>
class TestFunctionY16b32 : public TestFunction {

public:
	///	<summary>
	///		Evaluate the test function.
	///	</summary>
	virtual double operator()(
		double dLon,
		double dLat
	) {
		return (2.0 + pow(sin(2.0 * dLat), 16.0) * cos(16.0 * dLon));
		//return (2.0 + pow(cos(2.0 * dLat), 16.0) * cos(16.0 * dLon));
	}
};

///	<summary>
///		The constant function
///	</summary>
class TestFunction1 : public TestFunction {

public:
	///	<summary>
	///		Evaluate the test function.
	///	</summary>
	virtual double operator()(
		double dLon,
		double dLat
	) {
          return 1;
	}
};

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		Stationary vortex fields.
///	</summary>
class TestFunctionVortex : public TestFunction {

public:
	///	<summary>
	///		Find the rotated longitude and latitude of a point on a sphere
	///		with pole at (dLonC, dLatC).
	///	</summary>
	void RotatedSphereCoord(
		double dLonC,
		double dLatC,
		double & dLonT,
		double & dLatT
	) {
		double dSinC = sin(dLatC);
		double dCosC = cos(dLatC);
		double dCosT = cos(dLatT);
		double dSinT = sin(dLatT);
		
		double dTrm  = dCosT * cos(dLonT - dLonC);
		double dX = dSinC * dTrm - dCosC * dSinT;
		double dY = dCosT * sin(dLonT - dLonC);
		double dZ = dSinC * dSinT + dCosC * dTrm;

		dLonT = atan2(dY, dX);
		if (dLonT < 0.0) {
			dLonT += 2.0 * M_PI;
		}
		dLatT = asin(dZ);
	}

	///	<summary>
	///		Evaluate the test function.
	///	</summary>
	virtual double operator()(
		double dLon,
		double dLat
	) {
		const double dLon0 = 0.0;
		const double dLat0 = 0.6;
		const double dR0 = 3.0;
		const double dD = 5.0;
		const double dT = 6.0;

		RotatedSphereCoord(dLon0, dLat0, dLon, dLat);

		double dRho = dR0 * cos(dLat);
		double dVt = 3.0 * sqrt(3.0) / 2.0
			/ cosh(dRho) / cosh(dRho) * tanh(dRho);

		double dOmega;
		if (dRho == 0.0) {
			dOmega = 0.0;
		} else {
			dOmega = dVt / dRho;
		}

		return (1.0 - tanh(dRho / dD * sin(dLon - dOmega * dT)));
	}
};

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {

	NcError error(NcError::silent_nonfatal);

try {
	// Mesh file to use
	std::string strMeshFile;

	// Test data to use
	int iTestData;

	// Output on GLL grid
	bool fGLL;

	// Output on an integrated GLL grid
	bool fGLLIntegrate;

	// Degree of polynomial
	int nP;

	// Include a level dimension in output
	bool fHOMMEFormat;

	// Output variable name
	std::string strVariableName;

	// Output filename
	std::string strTestData;

	// Flip rectilinear ordering
	bool fFlipRectilinear;

	// Contains concave faces
	bool fContainsConcaveFaces;

	// Parse the command line
	BeginCommandLine()
		CommandLineString(strMeshFile, "mesh", "");
		CommandLineInt(iTestData, "test", 1);
		CommandLineBool(fGLL, "gll");
		CommandLineBool(fGLLIntegrate, "gllint");
		CommandLineInt(nP, "np", 4);
		CommandLineBool(fHOMMEFormat, "homme");
		CommandLineString(strVariableName, "var", "Psi");
		CommandLineString(strTestData, "out", "testdata.nc");
		CommandLineBool(fFlipRectilinear, "fliprectilinear");
		CommandLineBool(fContainsConcaveFaces, "concave");

		ParseCommandLine(argc, argv);
	EndCommandLine(argv)

	// Check arguments
	if (fGLLIntegrate && fGLL) {
		_EXCEPTIONT("--gll and --gllint are exclusive arguments");
	}

	// Announce
	Announce("=========================================================");

	// Triangular quadrature rule
	const int TriQuadratureOrder = 10;

	Announce("Using triangular quadrature of order %i", TriQuadratureOrder);

	TriangularQuadratureRule triquadrule(TriQuadratureOrder);

	const int TriQuadraturePoints = triquadrule.GetPoints();

	const DataArray2D<double> & TriQuadratureG = triquadrule.GetG();
	const DataArray1D<double> & TriQuadratureW = triquadrule.GetW();

	// Test data
	TestFunction * pTest;
	if (iTestData == 1) {
		pTest = new TestFunctionY2b2;
	} else if (iTestData == 2) {
		pTest = new TestFunctionY16b32;
	} else if (iTestData == 3) {
		pTest = new TestFunctionVortex;
	} else if (iTestData == 4) {
		pTest = new TestFunction1;
	} else {
		_EXCEPTIONT("Test index out of range; expected [1,2,3]");
	}

	// Input mesh
	AnnounceStartBlock("Loading Mesh");
	Mesh mesh(strMeshFile);

	// Check for rectilinear Mesh
	NcFile ncMesh(strMeshFile.c_str(), NcFile::ReadOnly);

	// Rectilinear grid
	bool fRectilinear = false;

	// Check for grid dimensions (SCRIP format grid)
	std::vector<long> vecOutputDimSizes;
	std::vector<std::string> vecOutputDimNames;

	NcVar * varGridDims = ncMesh.get_var("grid_dims");
	if (varGridDims != NULL) {
		NcDim * dimGridRank = varGridDims->get_dim(0);

		vecOutputDimSizes.resize(dimGridRank->size());
		varGridDims->get(&(vecOutputDimSizes[0]), dimGridRank->size());

		if (dimGridRank->size() == 1) {
			vecOutputDimNames.push_back("num_elem");
		} else if (dimGridRank->size() == 2) {
			fRectilinear = true;

			vecOutputDimNames.push_back("lon");
			vecOutputDimNames.push_back("lat");
		} else {
			_EXCEPTIONT("Source grid grid_rank must be < 3");
		}
	}

	// Check for rectilinear attribute (Exodus format grid)
	NcAtt * attRectilinear = ncMesh.get_att("rectilinear");
	if (attRectilinear != NULL) {
		fRectilinear = true;
		int nDim0Size = ncMesh.get_att("rectilinear_dim0_size")->as_int(0);
		int nDim1Size = ncMesh.get_att("rectilinear_dim1_size")->as_int(0);

		std::string strDim0Name =
			ncMesh.get_att("rectilinear_dim0_name")->as_string(0);
		std::string strDim1Name =
			ncMesh.get_att("rectilinear_dim1_name")->as_string(0);

		vecOutputDimSizes.push_back(nDim0Size);
		vecOutputDimSizes.push_back(nDim1Size);

		vecOutputDimNames.push_back(strDim0Name);
		vecOutputDimNames.push_back(strDim1Name);
	}

	// Default case
	if (vecOutputDimSizes.size() == 0) {
		vecOutputDimSizes.push_back(mesh.faces.size());
		vecOutputDimNames.push_back("ncol");
	}

	if (fRectilinear) {
		Announce("Rectilinear grid detected");
	} else {
		Announce("Non-rectilinear grid detected");
	}

	if (fFlipRectilinear && !fRectilinear) {
		_EXCEPTIONT("--fliprectlinear cannot be used with non-rectilinear grids");
	}

	if (fGLL && fRectilinear) {
		_EXCEPTIONT("--gll cannot be used with rectilinear grids");
	}

	// Output level dimension
	if (fHOMMEFormat) {
		vecOutputDimSizes.push_back(1);
		vecOutputDimNames.push_back("lev");
	}

	AnnounceEndBlock("Done");

	// Generate test data
	AnnounceStartBlock("Generating test data");

	// Latitude and Longitude arrays (used for HOMME format output)
	DataArray1D<double> dLat;
	DataArray1D<double> dLon;
	DataArray1D<double> dArea;

	// Output data
	DataArray1D<double> dVar;

	// Nodal geometric area
	DataArray1D<double> dNodeArea;

	// Calculate element areas
	mesh.CalculateFaceAreas(fContainsConcaveFaces);

	// Sample as element averages
	if ((!fGLLIntegrate) && (!fGLL)) {

		// Resize the array
		dVar.Allocate(mesh.faces.size());

		// Loop through all Faces
		for (int i = 0; i < mesh.faces.size(); i++) {

			const Face & face = mesh.faces[i];

			// Loop through all sub-triangles
			for (int j = 0; j < face.edges.size()-2; j++) {

				const Node & node0 = mesh.nodes[face[0]];
				const Node & node1 = mesh.nodes[face[j+1]];
				const Node & node2 = mesh.nodes[face[j+2]];

				// Triangle area
				Face faceTri(3);
				faceTri.SetNode(0, face[0]);
				faceTri.SetNode(1, face[j+1]);
				faceTri.SetNode(2, face[j+2]);

				double dTriangleArea = CalculateFaceArea(faceTri, mesh.nodes);

				// Calculate the element average
				double dTotalSample = 0.0;

				// Loop through all quadrature points
				for (int k = 0; k < TriQuadraturePoints; k++) {
					Node node(
						  TriQuadratureG[k][0] * node0.x
						+ TriQuadratureG[k][1] * node1.x
						+ TriQuadratureG[k][2] * node2.x,
						  TriQuadratureG[k][0] * node0.y
						+ TriQuadratureG[k][1] * node1.y
						+ TriQuadratureG[k][2] * node2.y,
						  TriQuadratureG[k][0] * node0.z
						+ TriQuadratureG[k][1] * node1.z
						+ TriQuadratureG[k][2] * node2.z);

					double dMagnitude = node.Magnitude();
					node.x /= dMagnitude;
					node.y /= dMagnitude;
					node.z /= dMagnitude;

					double dLon = atan2(node.y, node.x);
					if (dLon < 0.0) {
						dLon += 2.0 * M_PI;
					}
					double dLat = asin(node.z);

					double dSample = (*pTest)(dLon, dLat);

					dTotalSample += dSample * TriQuadratureW[k] * dTriangleArea;
				}

				// Flip the rectilinear coordinate
				int iv = i;
				if (fFlipRectilinear) {
					int i0 = i % vecOutputDimSizes[0];
					int i1 = i / vecOutputDimSizes[0];

					iv = i0 * vecOutputDimSizes[1] + i1;
				}
				dVar[iv] += dTotalSample / mesh.vecFaceArea[i];
				//dVar[i] += dTotalSample / mesh.vecFaceArea[i];
			}
		}

	// Finite element data
	} else {
		// Generate grid metadata
		DataArray3D<int> dataGLLNodes;
		DataArray3D<double> dataGLLJacobian;

		GenerateMetaData(mesh, nP, false, dataGLLNodes, dataGLLJacobian);

		// Number of elements
		int nElements = mesh.faces.size();

		// Verify all elements are quadrilaterals
		for (int k = 0; k < nElements; k++) {
			const Face & face = mesh.faces[k];

			if (face.edges.size() != 4) {
				_EXCEPTIONT("Non-quadrilateral face detected; "
					"incompatible with --gll");
			}
		}

		// Number of unique nodes
		int iMaxNode = 0;
		for (int i = 0; i < nP; i++) {
		for (int j = 0; j < nP; j++) {
		for (int k = 0; k < nElements; k++) {
			if (dataGLLNodes[i][j][k] > iMaxNode) {
				iMaxNode = dataGLLNodes[i][j][k];
			}
		}
		}
		}

		// Resize output array
		if (fHOMMEFormat) {
			vecOutputDimSizes[1] = iMaxNode;

			dLat.Allocate(iMaxNode);
			dLon.Allocate(iMaxNode);
			dArea.Allocate(iMaxNode);

		} else {
			vecOutputDimSizes[0] = iMaxNode;
		}

		// Get Gauss-Lobatto quadrature nodes
		DataArray1D<double> dG;
		DataArray1D<double> dW;

		GaussLobattoQuadrature::GetPoints(nP, 0.0, 1.0, dG, dW);

		// Get Gauss quadrature nodes
		const int nGaussP = 10;

		DataArray1D<double> dGaussG;
		DataArray1D<double> dGaussW;

		GaussQuadrature::GetPoints(nGaussP, 0.0, 1.0, dGaussG, dGaussW);

		// Allocate data
		dVar.Allocate(iMaxNode);
		dNodeArea.Allocate(iMaxNode);

		// Sample data
		for (int k = 0; k < nElements; k++) {

			const Face & face = mesh.faces[k];

			// Sample data at GLL nodes
			if (fGLL) {
				for (int i = 0; i < nP; i++) {
				for (int j = 0; j < nP; j++) {

					// Apply local map
					Node node;
					Node dDx1G;
					Node dDx2G;

					ApplyLocalMap(
						face,
						mesh.nodes,
						dG[i],
						dG[j],
						node,
						dDx1G,
						dDx2G);

					// Sample data at this point
					double dNodeLon = atan2(node.y, node.x);
					if (dNodeLon < 0.0) {
						dNodeLon += 2.0 * M_PI;
					}
					double dNodeLat = asin(node.z);

					double dSample = (*pTest)(dNodeLon, dNodeLat);

					dVar[dataGLLNodes[j][i][k]-1] = dSample;

					if (fHOMMEFormat) {
						dLat[dataGLLNodes[j][i][k]-1] = dNodeLat * 180.0 / M_PI;
						dLon[dataGLLNodes[j][i][k]-1] = dNodeLon * 180.0 / M_PI;
						dArea[dataGLLNodes[j][i][k]-1] += dataGLLJacobian[j][i][k];
					}
				}
				}

			// High-order Gaussian integration over basis function
			} else {
				DataArray2D<double> dCoeff(nP, nP);

				for (int p = 0; p < nGaussP; p++) {
				for (int q = 0; q < nGaussP; q++) {

					// Apply local map
					Node node;
					Node dDx1G;
					Node dDx2G;

					ApplyLocalMap(
						face,
						mesh.nodes,
						dGaussG[p],
						dGaussG[q],
						node,
						dDx1G,
						dDx2G);

					// Cross product gives local Jacobian
					Node nodeCross = CrossProduct(dDx1G, dDx2G);

					double dJacobian = sqrt(
						  nodeCross.x * nodeCross.x
						+ nodeCross.y * nodeCross.y
						+ nodeCross.z * nodeCross.z);

					// Find components of quadrature point in basis
					// of the first Face
					SampleGLLFiniteElement(
						0,
						nP,
						dGaussG[p],
						dGaussG[q],
						dCoeff);

					// Sample data at this point
					double dNodeLon = atan2(node.y, node.x);
					if (dNodeLon < 0.0) {
						dNodeLon += 2.0 * M_PI;
					}
					double dNodeLat = asin(node.z);

					double dSample = (*pTest)(dNodeLon, dNodeLat);

					// Integrate
					for (int i = 0; i < nP; i++) {
					for (int j = 0; j < nP; j++) {

						double dNodalArea =
							dCoeff[i][j]
							* dGaussW[p]
							* dGaussW[q]
							* dJacobian;

						dVar[dataGLLNodes[i][j][k]-1] +=
							dSample * dNodalArea;

						dNodeArea[dataGLLNodes[i][j][k]-1] +=
							dNodalArea;
					}
					}

				}
				}
			}
		}
	
		// Divide by area
		if (fGLLIntegrate) {
			for (int i = 0; i < dVar.GetRows(); i++) {
				dVar[i] /= dNodeArea[i];
			}
		}
	}

	AnnounceEndBlock("Done");

	// Output file
	AnnounceStartBlock("Writing results");

	NcFile ncOut(strTestData.c_str(), NcFile::Replace);

	// Add dimensions
	std::vector<NcDim *> vecDimOut;
	for (int d = 0; d < vecOutputDimSizes.size(); d++) {
		vecDimOut.push_back(
			ncOut.add_dim(
				vecOutputDimNames[d].c_str(),
				vecOutputDimSizes[d]));
	}

	// Add latitude and longitude variable
	if (fHOMMEFormat) {
		NcVar * varLat = ncOut.add_var("lat", ncDouble, vecDimOut[1]);
		NcVar * varLon = ncOut.add_var("lon", ncDouble, vecDimOut[1]);
		NcVar * varArea = ncOut.add_var("area", ncDouble, vecDimOut[1]);

		varLat->put(&(dLat[0]), vecOutputDimSizes[1]);
		varLon->put(&(dLon[0]), vecOutputDimSizes[1]);
		varArea->put(&(dArea[0]), vecOutputDimSizes[1]);
	}
/*
	// Output data
	if (fFlipRectilinear) {
		NcVar * varOut =
			ncOut.add_var(
				strVariableName.c_str(),
				ncDouble,
				vecDimOut[1],
				vecDimOut[0]);

		varOut->put(&(dVar[0]), vecOutputDimSizes[1], vecOutputDimSizes[0]);

	} else {
*/
		NcVar * varOut =
			ncOut.add_var(
				strVariableName.c_str(),
				ncDouble,
				static_cast<int>(vecOutputDimSizes.size()),
				(const NcDim**)&(vecDimOut[0]));

		varOut->put(&(dVar[0]), &(vecOutputDimSizes[0]));
	//}

	AnnounceEndBlock("Done");

	// Delete the test
	delete pTest;

	return (0);

} catch(Exception & e) {
	Announce(e.ToString().c_str());
	return (-1);

} catch(...) {
	return (-2);
}
}


///////////////////////////////////////////////////////////////////////////////


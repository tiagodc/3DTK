#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include "mesh/poisson.h"
#include "poisson/Ply.h"
#include "poisson/PoissonRecon.h"
#include "poisson/SurfaceTrimmer.h"

using namespace std;

Poisson::Poisson() {
  reset();
}

Poisson::~Poisson() {
  reset();
}

// called by (de)constructor or used for reset
void Poisson::reset() {
  reconstructed = 0;
  updated = 0;
  trimmed = 0;
  // reset points and normals only when they are not setted yet
  if (points.size() != 0 && normals.size() != 0) {
    for (int i = 0; i < points.size(); ++i) {
      delete [] points[i];
    }
    for (int i = 0; i < normals.size(); ++i) {
      delete [] normals[i];
    }
    points.clear();
    normals.clear();
  }
  for (int i = 0; i < vertices.size(); ++i) {
    delete [] vertices[i];
  }
  vertices.clear();
  for (int i = 0; i < faces.size(); ++i) {
    delete [] faces[i];
  }
  faces.clear();
  for (int i = 0; i < tVertices.size(); ++i) {
    delete [] tVertices[i];
  }
  tVertices.clear();
  for (int i = 0; i < tFaces.size(); ++i) {
    delete [] tFaces[i];
  }
  tFaces.clear();
  return;
}

int Poisson::setPoints(vector<vector<float>> &v) {
  reset();
  points.resize(v.size());
  for (int i = 0; i < points.size(); ++i) {
    points[i] = new float[3];
    points[i][0] = v[i][0]; 
    points[i][1] = v[i][1]; 
    points[i][2] = v[i][2];
  }
  return 1;
}

int Poisson::setNormals(vector<vector<float>> &n) {
  reset();
  normals.resize(n.size());
  for (int i = 0; i < normals.size(); ++i) {
    normals[i] = new float[3];
    normals[i][0] = n[i][0]; 
    normals[i][1] = n[i][1]; 
    normals[i][2] = n[i][2];
  }
  return 1;
}

int Poisson::setParams(PoissonParam &p) {
  Depth.set = true; Depth.value = p.Depth;
  return 1;
}

// export originally generated mesh by poisson
int Poisson::exportMesh(const char* modelPath) {
  if (!updated) {
    cout << "Fail to export mesh due to reconstruction failure" << endl;
    return 0;
  }
  fstream fs(modelPath, fstream::out);
  // get and write correct scaled points coordinates
  for (int i = 0; i < vertices.size(); ++i) {
    fs << "v " << vertices[i][0] << " " 
      << vertices[i][1] << " " 
      << vertices[i][2] << " " << endl;
  }
  // get and write correct ordered faces indexes
  for (int i = 0; i < faces.size(); ++i) {
    fs << "f " << faces[i][0] + 1 << " " 
      << faces[i][1] + 1 << " " 
      << faces[i][2] + 1 << " " << endl;
  }
  fs.close();
  return 1;
}

// export trimmed mesh generated by poisson
int Poisson::exportTrimmedMesh(const char* modelPath) {
  if (!trimmed) {
    cout << "Fail to export mesh due to trimming failure" << endl;
    return 0;
  }
  fstream fs(modelPath, fstream::out);
  // get and write correct scaled points coordinates
  for (int i = 0; i < tVertices.size(); ++i) {
    fs << "v " << tVertices[i][0] << " " 
      << tVertices[i][1] << " " 
      << tVertices[i][2] << " " << endl;
  }
  // get and write correct ordered faces indexes
  for (int i = 0; i < tFaces.size(); ++i) {
    fs << "f " << tFaces[i][0] + 1 << " " 
      << tFaces[i][1] + 1 << " " 
      << tFaces[i][2] + 1 << " " << endl;
  }
  fs.close();
  return 1;
}

// filter mesh with density value
// implementation in SurfaceTrimmer.h
int Poisson::surfaceTrimmer(float dstVal) {
  trimmed = CallSurfaceTrimmer(dstVal, vertices, faces, tVertices, tFaces);
  return trimmed;
}

int Poisson::apply() {
  if (points.size() == 0 || normals.size() == 0 || points.size() != normals.size()) {
    cout << "Fail to call poisson with wrong points and normals" << endl;
    return 0;
  }
  CoredFileMeshData<PlyValueVertex<float>> mesh;
  reconstructed = Execute< 2 , PlyValueVertex< Real > , true  >( points, normals, mesh );
  if (reconstructed) {
    mesh.resetIterator();
    // get vertices
    for (int i = 0; i < mesh.inCorePoints.size(); ++i) {
      float *v = new float[4];
      v[0] = mesh.inCorePoints[i].point.coords[0];
      v[1] = mesh.inCorePoints[i].point.coords[1];
      v[2] = mesh.inCorePoints[i].point.coords[2];
      v[3] = mesh.inCorePoints[i].value;
      vertices.push_back(v);
    }
    for (int i = 0; i < mesh.outOfCorePointCount(); ++i) {
      PlyValueVertex< Real > vt;
      mesh.nextOutOfCorePoint(vt);
      float *v = new float[4];
      v[0] = vt.point.coords[0];
      v[1] = vt.point.coords[1];
      v[2] = vt.point.coords[2];
      v[3] = vt.value;
      vertices.push_back(v);
    }
    // get faces
    for (int i = 0; i < mesh.polygonCount(); ++i) {
      int *f = new int[3];
      vector<CoredVertexIndex> face;
      mesh.nextPolygon(face);
      for (int j = 0; j < face.size(); ++j) {
        if (face[j].inCore) {
          f[j] = face[j].idx;
        }
        else {
          f[j] = face[j].idx + (int)(mesh.inCorePoints.size());
        }
      }
      faces.push_back(f);
    }
    updated = 1;
  }
  return reconstructed && updated;
}

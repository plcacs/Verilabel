bool DL_Dxf::handleLWPolylineData(DL_CreationInterface* /*creationInterface*/) {
    // Allocate LWPolyline vertices (group code 90):
    if (groupCode==90) {
        maxVertices = toInt(groupValue);
        if (maxVertices>0) {
            if (vertices!=NULL) {
                delete[] vertices;
            }
            vertices = new double[4*maxVertices];
            for (int i=0; i<maxVertices; ++i) {
                vertices[i*4] = 0.0;
                vertices[i*4+1] = 0.0;
                vertices[i*4+2] = 0.0;
                vertices[i*4+3] = 0.0;
            }
        }
        vertexIndex=-1;
        return true;
    }

    // Process LWPolylines vertices (group codes 10/20/30/42):
    else if (groupCode==10 || groupCode==20 ||
             groupCode==30 || groupCode==42) {

        if (vertexIndex<maxVertices-1 && groupCode==10) {
            vertexIndex++;
        }

        if (groupCode<=30) {
            if (vertexIndex>=0 && vertexIndex<maxVertices && vertexIndex>=0) {
                vertices[4*vertexIndex + (groupCode/10-1)] = toReal(groupValue);
            }
        } else if (groupCode==42 && vertexIndex<maxVertices && vertexIndex>=0) {
            vertices[4*vertexIndex + 3] = toReal(groupValue);
        }
        return true;
    }
    return false;
}
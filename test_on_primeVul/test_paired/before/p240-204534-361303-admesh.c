stl_remove_degenerate(stl_file *stl, int facet) {
  int edge1;
  int edge2;
  int edge3;
  int neighbor1;
  int neighbor2;
  int neighbor3;
  int vnot1;
  int vnot2;
  int vnot3;

  if (stl->error) return;

  if(   !memcmp(&stl->facet_start[facet].vertex[0],
                &stl->facet_start[facet].vertex[1], sizeof(stl_vertex))
        && !memcmp(&stl->facet_start[facet].vertex[1],
                   &stl->facet_start[facet].vertex[2], sizeof(stl_vertex))) {
    /* all 3 vertices are equal.  Just remove the facet.  I don't think*/
    /* this is really possible, but just in case... */
    printf("removing a facet in stl_remove_degenerate\n");

    stl_remove_facet(stl, facet);
    return;
  }

  if(!memcmp(&stl->facet_start[facet].vertex[0],
             &stl->facet_start[facet].vertex[1], sizeof(stl_vertex))) {
    edge1 = 1;
    edge2 = 2;
    edge3 = 0;
  } else if(!memcmp(&stl->facet_start[facet].vertex[1],
                    &stl->facet_start[facet].vertex[2], sizeof(stl_vertex))) {
    edge1 = 0;
    edge2 = 2;
    edge3 = 1;
  } else if(!memcmp(&stl->facet_start[facet].vertex[2],
                    &stl->facet_start[facet].vertex[0], sizeof(stl_vertex))) {
    edge1 = 0;
    edge2 = 1;
    edge3 = 2;
  } else {
    /* No degenerate. Function shouldn't have been called. */
    return;
  }
  neighbor1 = stl->neighbors_start[facet].neighbor[edge1];
  neighbor2 = stl->neighbors_start[facet].neighbor[edge2];

  if(neighbor1 == -1) {
    stl_update_connects_remove_1(stl, neighbor2);
  }
  if(neighbor2 == -1) {
    stl_update_connects_remove_1(stl, neighbor1);
  }


  neighbor3 = stl->neighbors_start[facet].neighbor[edge3];
  vnot1 = stl->neighbors_start[facet].which_vertex_not[edge1];
  vnot2 = stl->neighbors_start[facet].which_vertex_not[edge2];
  vnot3 = stl->neighbors_start[facet].which_vertex_not[edge3];

  if(neighbor1 != -1){
    stl->neighbors_start[neighbor1].neighbor[(vnot1 + 1) % 3] = neighbor2;
    stl->neighbors_start[neighbor1].which_vertex_not[(vnot1 + 1) % 3] = vnot2;
  }
  if(neighbor2 != -1){
    stl->neighbors_start[neighbor2].neighbor[(vnot2 + 1) % 3] = neighbor1;
    stl->neighbors_start[neighbor2].which_vertex_not[(vnot2 + 1) % 3] = vnot1;
  }

  stl_remove_facet(stl, facet);

  if(neighbor3 != -1) {
    stl_update_connects_remove_1(stl, neighbor3);
    stl->neighbors_start[neighbor3].neighbor[(vnot3 + 1) % 3] = -1;
  }
}
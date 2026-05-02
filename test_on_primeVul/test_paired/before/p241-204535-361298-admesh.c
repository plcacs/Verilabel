stl_update_connects_remove_1(stl_file *stl, int facet_num) {
  int j;

  if (stl->error) return;
  /* Update list of connected edges */
  j = ((stl->neighbors_start[facet_num].neighbor[0] == -1) +
       (stl->neighbors_start[facet_num].neighbor[1] == -1) +
       (stl->neighbors_start[facet_num].neighbor[2] == -1));
  if(j == 0) {		       /* Facet has 3 neighbors */
    stl->stats.connected_facets_3_edge -= 1;
  } else if(j == 1) {	     /* Facet has 2 neighbors */
    stl->stats.connected_facets_2_edge -= 1;
  } else if(j == 2) {	     /* Facet has 1 neighbor  */
    stl->stats.connected_facets_1_edge -= 1;
  }
}
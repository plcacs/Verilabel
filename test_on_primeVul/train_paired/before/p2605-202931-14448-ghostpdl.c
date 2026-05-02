  static void  Ins_IP( INS_ARG )
  {
    TT_F26Dot6  org_a, org_b, org_x,
                cur_a, cur_b, cur_x,
                distance;
    Int         point;
    (void)args;

    if ( CUR.top < CUR.GS.loop )
    {
      CUR.error = TT_Err_Invalid_Reference;
      return;
    }

    org_a = CUR_Func_dualproj( CUR.zp0.org_x[CUR.GS.rp1],
                               CUR.zp0.org_y[CUR.GS.rp1] );

    org_b = CUR_Func_dualproj( CUR.zp1.org_x[CUR.GS.rp2],
                               CUR.zp1.org_y[CUR.GS.rp2] );

    cur_a = CUR_Func_project( CUR.zp0.cur_x[CUR.GS.rp1],
                              CUR.zp0.cur_y[CUR.GS.rp1] );

    cur_b = CUR_Func_project( CUR.zp1.cur_x[CUR.GS.rp2],
                              CUR.zp1.cur_y[CUR.GS.rp2] );

    while ( CUR.GS.loop > 0 )
    {
      CUR.args--;

      point = (Int)CUR.stack[CUR.args];
      if ( BOUNDS( point, CUR.zp2.n_points ) )
      {
        CUR.error = TT_Err_Invalid_Reference;
        return;
      }

      org_x = CUR_Func_dualproj( CUR.zp2.org_x[point],
                                 CUR.zp2.org_y[point] );

      cur_x = CUR_Func_project( CUR.zp2.cur_x[point],
                                CUR.zp2.cur_y[point] );

      if ( ( org_a <= org_b && org_x <= org_a ) ||
           ( org_a >  org_b && org_x >= org_a ) )

        distance = ( cur_a - org_a ) + ( org_x - cur_x );

      else if ( ( org_a <= org_b  &&  org_x >= org_b ) ||
                ( org_a >  org_b  &&  org_x <  org_b ) )

        distance = ( cur_b - org_b ) + ( org_x - cur_x );

      else
         /* note: it seems that rounding this value isn't a good */
         /*       idea (cf. width of capital 'S' in Times)       */

         distance = MulDiv_Round( cur_b - cur_a,
                                  org_x - org_a,
                                  org_b - org_a ) + ( cur_a - cur_x );

      CUR_Func_move( &CUR.zp2, point, distance );

      CUR.GS.loop--;
    }

    CUR.GS.loop = 1;
    CUR.new_top = CUR.args;
  }
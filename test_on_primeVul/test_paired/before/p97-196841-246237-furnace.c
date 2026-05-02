inline void FurnaceGUI::patternRow(int i, bool isPlaying, float lineHeight, int chans, int ord, const DivPattern** patCache) {
  static char id[32];
  bool selectedRow=(i>=sel1.y && i<=sel2.y);
  ImGui::TableNextRow(0,lineHeight);
  ImGui::TableNextColumn();
  float cursorPosY=ImGui::GetCursorPos().y-ImGui::GetScrollY();
  // check if the row is visible
  if (cursorPosY<-lineHeight || cursorPosY>ImGui::GetWindowSize().y) {
    return;
  }
  // check if we are in range
  if (ord<0 || ord>=e->song.ordersLen) {
    return;
  }
  if (i<0 || i>=e->song.patLen) {
    return;
  }
  bool isPushing=false;
  ImVec4 activeColor=uiColors[GUI_COLOR_PATTERN_ACTIVE];
  ImVec4 inactiveColor=uiColors[GUI_COLOR_PATTERN_INACTIVE];
  ImVec4 rowIndexColor=uiColors[GUI_COLOR_PATTERN_ROW_INDEX];
  if (e->song.hilightB>0 && !(i%e->song.hilightB)) {
    activeColor=uiColors[GUI_COLOR_PATTERN_ACTIVE_HI2];
    inactiveColor=uiColors[GUI_COLOR_PATTERN_INACTIVE_HI2];
    rowIndexColor=uiColors[GUI_COLOR_PATTERN_ROW_INDEX_HI2];
  } else if (e->song.hilightA>0 && !(i%e->song.hilightA)) {
    activeColor=uiColors[GUI_COLOR_PATTERN_ACTIVE_HI1];
    inactiveColor=uiColors[GUI_COLOR_PATTERN_INACTIVE_HI1];
    rowIndexColor=uiColors[GUI_COLOR_PATTERN_ROW_INDEX_HI1];
  }
  // check overflow highlight
  if (settings.overflowHighlight) {
    if (edit && cursor.y==i) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(uiColors[GUI_COLOR_EDITING]));
    } else if (isPlaying && oldRow==i) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(uiColors[GUI_COLOR_PATTERN_PLAY_HEAD]));
    } else if (e->song.hilightB>0 && !(i%e->song.hilightB)) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(uiColors[GUI_COLOR_PATTERN_HI_2]));
    } else if (e->song.hilightA>0 && !(i%e->song.hilightA)) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(uiColors[GUI_COLOR_PATTERN_HI_1]));
    }
  } else {
    isPushing=true;
    if (edit && cursor.y==i) {
      ImGui::PushStyleColor(ImGuiCol_Header,ImGui::GetColorU32(uiColors[GUI_COLOR_EDITING]));
    } else if (isPlaying && oldRow==i) {
      ImGui::PushStyleColor(ImGuiCol_Header,ImGui::GetColorU32(uiColors[GUI_COLOR_PATTERN_PLAY_HEAD]));
    } else if (e->song.hilightB>0 && !(i%e->song.hilightB)) {
      ImGui::PushStyleColor(ImGuiCol_Header,ImGui::GetColorU32(uiColors[GUI_COLOR_PATTERN_HI_2]));
    } else if (e->song.hilightA>0 && !(i%e->song.hilightA)) {
      ImGui::PushStyleColor(ImGuiCol_Header,ImGui::GetColorU32(uiColors[GUI_COLOR_PATTERN_HI_1]));
    } else {
      isPushing=false;
    }
  }
  // row number
  if (settings.patRowsBase==1) {
    ImGui::TextColored(rowIndexColor," %.2X ",i);
  } else {
    ImGui::TextColored(rowIndexColor,"%3d ",i);
  }
  // for each column
  for (int j=0; j<chans; j++) {
    // check if channel is not hidden
    if (!e->song.chanShow[j]) {
      patChanX[j]=ImGui::GetCursorPosX();
      continue;
    }
    int chanVolMax=e->getMaxVolumeChan(j);
    if (chanVolMax<1) chanVolMax=1;
    const DivPattern* pat=patCache[j];
    ImGui::TableNextColumn();
    patChanX[j]=ImGui::GetCursorPosX();

    // selection highlight flags
    int sel1XSum=sel1.xCoarse*32+sel1.xFine;
    int sel2XSum=sel2.xCoarse*32+sel2.xFine;
    int j32=j*32;
    bool selectedNote=selectedRow && (j32>=sel1XSum && j32<=sel2XSum);
    bool selectedIns=selectedRow && (j32+1>=sel1XSum && j32+1<=sel2XSum);
    bool selectedVol=selectedRow && (j32+2>=sel1XSum && j32+2<=sel2XSum);
    bool cursorNote=(cursor.y==i && cursor.xCoarse==j && cursor.xFine==0);
    bool cursorIns=(cursor.y==i && cursor.xCoarse==j && cursor.xFine==1);
    bool cursorVol=(cursor.y==i && cursor.xCoarse==j && cursor.xFine==2);

    // note
    sprintf(id,"%s##PN_%d_%d",noteName(pat->data[i][0],pat->data[i][1]),i,j);
    if (pat->data[i][0]==0 && pat->data[i][1]==0) {
      ImGui::PushStyleColor(ImGuiCol_Text,inactiveColor);
    } else {
      ImGui::PushStyleColor(ImGuiCol_Text,activeColor);
    }
    if (cursorNote) {
      ImGui::PushStyleColor(ImGuiCol_Header,uiColors[GUI_COLOR_PATTERN_CURSOR]);
      ImGui::PushStyleColor(ImGuiCol_HeaderActive,uiColors[GUI_COLOR_PATTERN_CURSOR_ACTIVE]);
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered,uiColors[GUI_COLOR_PATTERN_CURSOR_HOVER]);
      ImGui::Selectable(id,true,ImGuiSelectableFlags_NoPadWithHalfSpacing,threeChars);
      demandX=ImGui::GetCursorPosX();
      ImGui::PopStyleColor(3);
    } else {
      if (selectedNote) ImGui::PushStyleColor(ImGuiCol_Header,uiColors[GUI_COLOR_PATTERN_SELECTION]);
      ImGui::Selectable(id,isPushing || selectedNote,ImGuiSelectableFlags_NoPadWithHalfSpacing,threeChars);
      if (selectedNote) ImGui::PopStyleColor();
    }
    if (ImGui::IsItemClicked()) {
      startSelection(j,0,i);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
      updateSelection(j,0,i);
    }
    ImGui::PopStyleColor();

    // the following is only visible when the channel is not collapsed
    if (!e->song.chanCollapse[j]) {
      // instrument
      if (pat->data[i][2]==-1) {
        ImGui::PushStyleColor(ImGuiCol_Text,inactiveColor);
        sprintf(id,"..##PI_%d_%d",i,j);
      } else {
        if (pat->data[i][2]<0 || pat->data[i][2]>=e->song.insLen) {
          ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_INS_ERROR]);
        } else {
          DivInstrumentType t=e->song.ins[pat->data[i][2]]->type;
          if (t!=DIV_INS_AMIGA && t!=e->getPreferInsType(j)) {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_INS_WARN]);
          } else {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_INS]);
          }
        }
        sprintf(id,"%.2X##PI_%d_%d",pat->data[i][2],i,j);
      }
      ImGui::SameLine(0.0f,0.0f);
      if (cursorIns) {
        ImGui::PushStyleColor(ImGuiCol_Header,uiColors[GUI_COLOR_PATTERN_CURSOR]);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,uiColors[GUI_COLOR_PATTERN_CURSOR_ACTIVE]);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,uiColors[GUI_COLOR_PATTERN_CURSOR_HOVER]);
        ImGui::Selectable(id,true,ImGuiSelectableFlags_NoPadWithHalfSpacing,twoChars);
        demandX=ImGui::GetCursorPosX();
        ImGui::PopStyleColor(3);
      } else {
        if (selectedIns) ImGui::PushStyleColor(ImGuiCol_Header,uiColors[GUI_COLOR_PATTERN_SELECTION]);
        ImGui::Selectable(id,isPushing || selectedIns,ImGuiSelectableFlags_NoPadWithHalfSpacing,twoChars);
        if (selectedIns) ImGui::PopStyleColor();
      }
      if (ImGui::IsItemClicked()) {
        startSelection(j,1,i);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
        updateSelection(j,1,i);
      }
      ImGui::PopStyleColor();

      // volume
      if (pat->data[i][3]==-1) {
        sprintf(id,"..##PV_%d_%d",i,j);
        ImGui::PushStyleColor(ImGuiCol_Text,inactiveColor);
      } else {
        int volColor=(pat->data[i][3]*127)/chanVolMax;
        if (volColor>127) volColor=127;
        if (volColor<0) volColor=0;
        sprintf(id,"%.2X##PV_%d_%d",pat->data[i][3],i,j);
        ImGui::PushStyleColor(ImGuiCol_Text,volColors[volColor]);
      }
      ImGui::SameLine(0.0f,0.0f);
      if (cursorVol) {
        ImGui::PushStyleColor(ImGuiCol_Header,uiColors[GUI_COLOR_PATTERN_CURSOR]);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,uiColors[GUI_COLOR_PATTERN_CURSOR_ACTIVE]);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,uiColors[GUI_COLOR_PATTERN_CURSOR_HOVER]);
        ImGui::Selectable(id,true,ImGuiSelectableFlags_NoPadWithHalfSpacing,twoChars);
        demandX=ImGui::GetCursorPosX();
        ImGui::PopStyleColor(3);
      } else {
        if (selectedVol) ImGui::PushStyleColor(ImGuiCol_Header,uiColors[GUI_COLOR_PATTERN_SELECTION]);
        ImGui::Selectable(id,isPushing || selectedVol,ImGuiSelectableFlags_NoPadWithHalfSpacing,twoChars);
        if (selectedVol) ImGui::PopStyleColor();
      }
      if (ImGui::IsItemClicked()) {
        startSelection(j,2,i);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
        updateSelection(j,2,i);
      }
      ImGui::PopStyleColor();

      // effects
      for (int k=0; k<e->song.pat[j].effectRows; k++) {
        int index=4+(k<<1);
        bool selectedEffect=selectedRow && (j32+index-1>=sel1XSum && j32+index-1<=sel2XSum);
        bool selectedEffectVal=selectedRow && (j32+index>=sel1XSum && j32+index<=sel2XSum);
        bool cursorEffect=(cursor.y==i && cursor.xCoarse==j && cursor.xFine==index-1);
        bool cursorEffectVal=(cursor.y==i && cursor.xCoarse==j && cursor.xFine==index);
        
        // effect
        if (pat->data[i][index]==-1) {
          sprintf(id,"..##PE%d_%d_%d",k,i,j);
          ImGui::PushStyleColor(ImGuiCol_Text,inactiveColor);
        } else {
          sprintf(id,"%.2X##PE%d_%d_%d",pat->data[i][index],k,i,j);
          if (pat->data[i][index]<0x10) {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[fxColors[pat->data[i][index]]]);
          } else if (pat->data[i][index]<0x20) {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_EFFECT_SYS_PRIMARY]);
          } else if (pat->data[i][index]<0x30) {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_EFFECT_SYS_SECONDARY]);
          } else if (pat->data[i][index]<0x48) {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_EFFECT_SYS_PRIMARY]);
          } else if (pat->data[i][index]<0x90) {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_EFFECT_INVALID]);
          } else if (pat->data[i][index]<0xa0) {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_EFFECT_MISC]);
          } else if (pat->data[i][index]<0xc0) {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_EFFECT_INVALID]);
          } else if (pat->data[i][index]<0xd0) {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_EFFECT_SPEED]);
          } else if (pat->data[i][index]<0xe0) {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[GUI_COLOR_PATTERN_EFFECT_INVALID]);
          } else {
            ImGui::PushStyleColor(ImGuiCol_Text,uiColors[extFxColors[pat->data[i][index]-0xe0]]);
          }
        }
        ImGui::SameLine(0.0f,0.0f);
        if (cursorEffect) {
          ImGui::PushStyleColor(ImGuiCol_Header,uiColors[GUI_COLOR_PATTERN_CURSOR]);  
          ImGui::PushStyleColor(ImGuiCol_HeaderActive,uiColors[GUI_COLOR_PATTERN_CURSOR_ACTIVE]);
          ImGui::PushStyleColor(ImGuiCol_HeaderHovered,uiColors[GUI_COLOR_PATTERN_CURSOR_HOVER]);
          ImGui::Selectable(id,true,ImGuiSelectableFlags_NoPadWithHalfSpacing,twoChars);
          demandX=ImGui::GetCursorPosX();
          ImGui::PopStyleColor(3);
        } else {
          if (selectedEffect) ImGui::PushStyleColor(ImGuiCol_Header,uiColors[GUI_COLOR_PATTERN_SELECTION]);
          ImGui::Selectable(id,isPushing || selectedEffect,ImGuiSelectableFlags_NoPadWithHalfSpacing,twoChars);
          if (selectedEffect) ImGui::PopStyleColor();
        }
        if (ImGui::IsItemClicked()) {
          startSelection(j,index-1,i);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
          updateSelection(j,index-1,i);
        }

        // effect value
        if (pat->data[i][index+1]==-1) {
          sprintf(id,"..##PF%d_%d_%d",k,i,j);
        } else {
          sprintf(id,"%.2X##PF%d_%d_%d",pat->data[i][index+1],k,i,j);
        }
        ImGui::SameLine(0.0f,0.0f);
        if (cursorEffectVal) {
          ImGui::PushStyleColor(ImGuiCol_Header,uiColors[GUI_COLOR_PATTERN_CURSOR]);  
          ImGui::PushStyleColor(ImGuiCol_HeaderActive,uiColors[GUI_COLOR_PATTERN_CURSOR_ACTIVE]);
          ImGui::PushStyleColor(ImGuiCol_HeaderHovered,uiColors[GUI_COLOR_PATTERN_CURSOR_HOVER]);
          ImGui::Selectable(id,true,ImGuiSelectableFlags_NoPadWithHalfSpacing,twoChars);
          demandX=ImGui::GetCursorPosX();
          ImGui::PopStyleColor(3);
        } else {
          if (selectedEffectVal) ImGui::PushStyleColor(ImGuiCol_Header,uiColors[GUI_COLOR_PATTERN_SELECTION]);
          ImGui::Selectable(id,isPushing || selectedEffectVal,ImGuiSelectableFlags_NoPadWithHalfSpacing,twoChars);
          if (selectedEffectVal) ImGui::PopStyleColor();
        }
        if (ImGui::IsItemClicked()) {
          startSelection(j,index,i);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
          updateSelection(j,index,i);
        }
        ImGui::PopStyleColor();
      }
    }
  }
  if (isPushing) {
    ImGui::PopStyleColor();
  }
  ImGui::TableNextColumn();
  patChanX[chans]=ImGui::GetCursorPosX();
}
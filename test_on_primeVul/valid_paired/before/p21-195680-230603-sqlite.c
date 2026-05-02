void sqlite3Fts5UnicodeAscii(u8 *aArray, u8 *aAscii){
  int i = 0;
  int iTbl = 0;
  while( i<128 ){
    int bToken = aArray[ aFts5UnicodeData[iTbl] & 0x1F ];
    int n = (aFts5UnicodeData[iTbl] >> 5) + i;
    for(; i<128 && i<n; i++){
      aAscii[i] = (u8)bToken;
    }
    iTbl++;
  }
}
// NP relation to prove weight of MST in a graph is < 15

extern "C" {

inline int oblivious_if(bool cond, int t, int f) {
    int mask = static_cast<int>((1ULL << 33) - cond);
    return (mask & t) | (~mask & f);
}

int minwtSpanningTree(const char* word1, const char* word2, const int m, const int n) {
    int pre;
    int wt[m/6];
    int lnode[m/6];
    int rnode[m/6];
    int spt[n];
    int spt_left[n];
    int spt_right[n];
    char *end;
    int v=0;
    // read word1 into tuples (x,y,w) which denotes an edge between x and y with weight w. all numbers are encoded in three digits. 
    // 003006777 means an edge between node 3 and node 6 with weight 777. word1 contains a concatenation of the encoding of all edges/weight of the graph
    for(int j=0;j<m;j+=9)
    {
	    lnode[j/9] = 100*(word1[j]-'0')+10*(word1[j+1]-'0')+(word1[j+2]-'0');
	    rnode[j/9] = 100*(word1[j+3]-'0')+10*(word1[j+4]-'0')+(word1[j+5]-'0');
	    wt[j/9] = 100*(word1[j+6]-'0')+10*(word1[j+7]-'0')+(word1[j+8]-'0');
    }
    int e = m/9;
    // Sort the edges on weight using bubble sort
    for(int i = 0; i < e;i++)
    {
	    for(int j = 0; j < e - 1;j++)
	    {
		    bool cond = wt[j] > wt[j+1];

		    int temp = wt[j];
		    wt[j] = oblivious_if(cond,wt[j+1],temp);
		    wt[j+1] = oblivious_if(cond,temp,wt[j+1]);
		    
		    temp = lnode[j];
		    lnode[j] = oblivious_if(cond,lnode[j+1],temp);
		    lnode[j+1] = oblivious_if(cond,temp,lnode[j+1]);
		    
		    temp = rnode[j];
		    rnode[j] = oblivious_if(cond,rnode[j+1],temp);
		    rnode[j+1] = oblivious_if(cond,temp,rnode[j+1]);
	    }
    }

    //add smallest weight edge to MST
    spt[0] = wt[0];
    spt_left[0] = lnode[0];
    spt_right[0] = rnode[0];
    int pos=1;
    //Run Kruskal's algorithm
    for(int i = 1; i < e ; i++)
    {
	    bool lcond = false;
	    bool rcond = false;
            //Check if next edge creates a cycle
	    for(int j = 0; j < i; j++)
	    {
		    lcond = lcond || (lnode[i] == lnode[j]) || (lnode[i] == rnode[j]);
		    rcond = rcond || (rnode[i] == lnode[j]) || (rnode[i] == rnode[j]);
	    }
            //Insert edge into MST if it does not form a cycle
	    for(int k = 0; k < n; k++)
	    {
		    bool cond = (lcond && rcond) || (k != pos);
		    spt[k] = oblivious_if(cond,spt[k],wt[i]);
		    spt_left[k] = oblivious_if(cond,spt_left[k],lnode[i]);
		    spt_right[k] = oblivious_if(cond,spt_right[k],rnode[i]);
	    }
	    pos = oblivious_if(lcond && rcond, pos, pos+1);

    }
    //compute weight of MST
    int totalwt=0;
    for(int i = 0; i < n; i++)
    {
	    totalwt = totalwt + spt[i];
    }

    return totalwt;
}


bool statement(const char *word1, const char* word2, const int m, const int n) {
    return minwtSpanningTree(word1, word2, m, n) < 15;
}


}

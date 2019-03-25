
/*
./c cclib rng 17 \"[%229433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775%22,250]\"
{
    "playerid": 250,
    "seed": 1398876319979341887,
    "rng": 14565767519458298868,
    "lastrng": 15075236803740723044,
    "maxrngs": 10000,
    "result": "success"
}

 ./c cclib rngnext 17 \"[14565767519458298868]\"
 {
 "seed": 14565767519458297856,
 "rng": 4253087318999719449,
 "result": "success"
 }
 
 The idea is for a game to start with a near future blockhash, so the lobby gets players signed up until just prior to the designated height. then that blockhash can be used to create a stream of rngs.
 
 the same initial rng can be used for all players, if the identical starting condition is required. up to 255 different initial rng can be derived from a single blockhash. (Actually any number is possible, for simplicity rng rpc limits to 255).
 
 you will notice maxrngs and lastrng, the lastrng is the rng value that will happen after maxrng iterations of calling rngnext with the current rng. This allows making time based multiplayer games where all the nodes can validate all the other nodes rng, even without realtime synchronization of all user input events.
 
 Every time period, all players would set their rng value to the lastrng value. The only thing to be careful of is it not exceed the maxrng calls to rngnext in a single time period. otherwise the same set of rng numbers will be repeated.
*/


CScript games_opret(uint8_t funcid,CPubKey pk)
{
    CScript opret; uint8_t evalcode = EVAL_GAMES;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << pk);
    return(opret);
}

uint8_t games_opretdecode(CPubKey &pk,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> pk) != 0 && e == EVAL_GAMES )
    {
        return(f);
    }
    return(0);
}

UniValue games_rawtxresult(UniValue &result,std::string rawtx,int32_t broadcastflag)
{
    CTransaction tx;
    if ( rawtx.size() > 0 )
    {
        result.push_back(Pair("hex",rawtx));
        if ( DecodeHexTx(tx,rawtx) != 0 )
        {
            if ( broadcastflag != 0 && myAddtomempool(tx) != 0 )
                RelayTransaction(tx);
            result.push_back(Pair("txid",tx.GetHash().ToString()));
            result.push_back(Pair("result","success"));
        } else result.push_back(Pair("error","decode hex"));
    } else result.push_back(Pair("error","couldnt finalize CCtx"));
    return(result);
}

uint64_t _games_rngnext(uint64_t initseed)
{
    uint16_t seeds[4]; int32_t i;
    seeds[0] = initseed;
    seeds[1] = (initseed >> 16);
    seeds[2] = (initseed >> 32);
    seeds[3] = (initseed >> 48);
    seeds[0] = (seeds[0]*GAMES_RNGMULT + GAMES_RNGOFFSET);
    seeds[1] = ((seeds[0] ^ seeds[1])*GAMES_RNGMULT + GAMES_RNGOFFSET);
    seeds[2] = ((seeds[0] ^ seeds[1] ^ seeds[2])*GAMES_RNGMULT + GAMES_RNGOFFSET);
    seeds[3] = ((seeds[0] ^ seeds[1] ^ seeds[2] ^ seeds[3])*GAMES_RNGMULT + GAMES_RNGOFFSET);
    return(((uint64_t)seeds[3] << 48) | ((uint64_t)seeds[2] << 24) | ((uint64_t)seeds[1] << 16) | seeds[0]);
}

UniValue games_rngnext(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t n; uint64_t seed;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 1 )
    {
        seed = jdouble(jitem(params,0),0);
        result.push_back(Pair("seed",seed));
        seed = _games_rngnext(seed);
        result.push_back(Pair("rng",seed));
        result.push_back(Pair("result","success"));
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","not enough params"));
    }
    return(result);
}

UniValue games_rng(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t i,n,playerid=0; uint64_t seed=0,initseed; bits256 hash;
    if ( params != 0 && ((n= cJSON_GetArraySize(params)) == 1 || n == 2) )
    {
        hash = jbits256(jitem(params,0),0);
        if ( n == 2 )
        {
            playerid = juint(jitem(params,1),0);
            if ( playerid >= 0xff )
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","playerid too big"));
                return(result);
            }
        }
        playerid++;
        for (i=0; i<8; i++)
        {
            if ( ((1 << i) & playerid) != 0 )
                seed ^= (uint64_t)hash.uints[i] << ((i&1)*32);
        }
        initseed = seed;
        seed = _games_rngnext(initseed);
        result.push_back(Pair("playerid",(int64_t)(playerid - 1)));
        result.push_back(Pair("seed",initseed));
        result.push_back(Pair("rng",seed));
        for (i=0; i<GAMES_MAXRNGS; i++)
            seed = _games_rngnext(seed);
        result.push_back(Pair("lastrng",seed));
        result.push_back(Pair("maxrngs",GAMES_MAXRNGS));
        result.push_back(Pair("result","success"));
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","not enough params"));
    }
    return(result);
}

UniValue games_events(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); std::vector<uint8_t> payload; int32_t n;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 1 )
    {
        if ( payments_parsehexdata(payload,jitem(params,0),0) == 0 )
        {
            komodo_sendmessage(4,8,"events",payload);
            result.push_back(Pair("result","success"));
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","couldnt parsehexdata"));
        }
   }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","not enough params"));
    }
    return(result);
}

UniValue games_create(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    return(result);
}

UniValue games_info(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    return(result);
}

UniValue games_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    return(result);
}

void komodo_netevent(std::vector<uint8_t> payload)
{
    int32_t i;
    for (i=0; i<payload.size(); i++)
        fprintf(stderr,"%02x",payload[i]);
    fprintf(stderr," got event[%d]\n",(int32_t)payload.size());
}

bool games_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    return(true);
}



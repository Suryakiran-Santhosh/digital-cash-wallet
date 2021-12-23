#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AccountService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

void response_creation(string, int, HTTPResponse*);

AccountService::AccountService() : HttpService("/users") {
  
}

void AccountService::get(HTTPRequest *request, HTTPResponse *response){
    User* user = getAuthenticatedUser(request);
    string user_id = "";

    if(user == NULL){
        throw ClientError::unauthorized();
    }

    // get path componenets size too small then error
    vector<string> path = request->getPathComponents();
    if(path.size() < 2){
        throw ClientError::badRequest();
    }
    user_id = path[1];

    if(user->user_id != user_id){
        throw ClientError::unauthorized();
    }

    string email = user->email;
    int balance = user->balance;

    response_creation(email, balance, response);
}


void AccountService::put(HTTPRequest *request, HTTPResponse *response){
    WwwFormEncodedDict body = request->formEncodedBody();
    string email = body.get("email");

    User* user = getAuthenticatedUser(request);
    if(user == NULL){
        throw ClientError::unauthorized();
    }

    vector<string> path = request->getPathComponents();
    if(path.size() < 2){
        throw ClientError::badRequest();
    }
    string user_id = path[1];
    if(user_id.empty() ){
        throw ClientError::badRequest();
    }

    if(user->user_id != user_id){
        throw ClientError::forbidden();
    }

    if(email.empty()){
        throw ClientError::badRequest();
    }

    user->email = email;
    int balance = user->balance;

    response_creation(email, balance, response);
}


void response_creation(string email, int balance, HTTPResponse* response){
    Document document;
    Document::AllocatorType &allocator = document.GetAllocator();

    Value user_info;
    user_info.SetObject();
    user_info.AddMember("email", email, allocator);
    user_info.AddMember("balance", balance, allocator);
    document.Swap(user_info);

    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);

    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n"));
}
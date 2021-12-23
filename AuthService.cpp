#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <utility>
#include <ctype.h>

#include "AuthService.h"
#include "StringUtils.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;


void create_response(string token, string id, HTTPResponse* response);

AuthService::AuthService() : HttpService("/auth-tokens") {

}

void AuthService::post(HTTPRequest *request, HTTPResponse *response) {

	// form encoded body contains information regarding this is a Map aka Dictionary
	WwwFormEncodedDict account_information = request->formEncodedBody();
	string username = account_information.get("username");  // username of the request user
	string password = account_information.get("password");
	string email = account_information.get("email");	

	if (username.empty() || password.empty()){
		throw ClientError::badRequest();
	}

	for(size_t i = 0; i < username.size(); i++){
		if (!islower(username[i])){
			throw ClientError::badRequest();
		}
	}
	
	if (m_db->users.count(username) == 0){
		// checks to see if the username exits in the database
		User* new_user = new User();
		new_user->username = username;
		new_user->password = password;
		new_user->balance = 0;
		if(!email.empty()){
			new_user->email = email;
		}
		else{
			new_user->email = "";
		}
		
		new_user->user_id = "";

		// store user in database
		pair<string, User*> user_to_add_to_database(new_user->username, new_user);
		m_db->users.insert(user_to_add_to_database);

		// create auth token and  user id and pass to database
		StringUtils *obj = new StringUtils();
		string token = obj->createAuthToken();
		new_user->user_id = obj->createUserId();

		// User* user_account = m_db->users.at(username);
		pair<string, User*> new_auth_token (token, new_user);
		m_db->auth_tokens.insert(new_auth_token);
		
		create_response(token, new_user->user_id, response);
		response->setStatus(201);
	}
	else{
		// IN THE CASE OF AN EXISTING USER
		// check if the user is in the database and if they are then log them in then
		if (m_db->users.at(username)->password != password){
			// the case in which the user is in the database but the user did not input the correct password
			throw ClientError::forbidden();
		}
		else{
			// user inputs the correct password therefore log in the user
			StringUtils obj;
			string token = obj.createAuthToken();
			
			User* user_account = m_db->users.at(username);
			pair<string, User*> new_auth_token (token, user_account);
			m_db->auth_tokens.insert(new_auth_token);
			
			create_response(token, user_account->user_id, response);
			response->setStatus(200);
		}   
	}
}


// deleting auth token signs out user
void AuthService::del(HTTPRequest *request, HTTPResponse *response) {
	// PROCESS:
	// authenticate requester 
	// get auth token from the http request
	// delete that token from the http request
	// set response setStatus
	if(getAuthenticatedUser(request) == NULL){
		throw ClientError::unauthorized();
		return;
	}

	vector<string> path = request->getPathComponents();
	string delete_token = path.at(1);
	if(m_db->auth_tokens.count(delete_token) > 0){
		m_db->auth_tokens.erase(delete_token);
	}
}


void create_response(string token, string id, HTTPResponse* response){
	Document document;
	Document::AllocatorType &allocator = document.GetAllocator();

	Value user_info;
	user_info.SetObject();
	user_info.AddMember("auth_token", token, allocator);
	user_info.AddMember("user_id", id, allocator);

	document.Swap(user_info);
	StringBuffer buffer;
	PrettyWriter<StringBuffer> writer(buffer);
	document.Accept(writer);

	response->setContentType("application/json");
	response->setBody(buffer.GetString() + string("\n"));
}
#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "TransferService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

// void response_creation(int, vector<Transfer*>, HTTPResponse*);

TransferService::TransferService() : HttpService("/transfers") { 

}


/*
 * missing amount or to argument %
 * transfer amount is negative %
 * transfer amount is greater than balance %
 * to user doesn't exist in the users database %
*/
void TransferService::post(HTTPRequest *request, HTTPResponse *response) {
	WwwFormEncodedDict body = request->formEncodedBody();
	string recipient_username = body.get("to");
	int amount = stoi(body.get("amount"));

	if(recipient_username.empty()){
		throw ClientError::badRequest();
	}

	// getauthtoken from token
	User* sender = getAuthenticatedUser(request);
	User* recipient = NULL;

	// authenticate sender
	if(sender == NULL){
		throw ClientError::unauthorized();
	}

	// authenticate recipient
	if(m_db->users.count(recipient_username) == 0){
		throw ClientError::unauthorized();
	}
	else{
		recipient = m_db->users.at(recipient_username);
	}

	if(amount < 50 || amount < 0){
		throw ClientError::badRequest();
	}
	
	if(amount > sender->balance){
		throw ClientError::forbidden();
	}

	sender->balance -= amount;
	recipient->balance += amount;

	// update transfer log
	Transfer* current_transfer;
	current_transfer->from  = sender;
	current_transfer->to = recipient;
	current_transfer->amount = amount;
	m_db->transfers.push_back(current_transfer);  // update database
	
	// json creation
	Document document;
	Document::AllocatorType &allocator = document.GetAllocator();

	// user balance element for json
	Value info; // return object
	info.SetObject();
	info.AddMember("balance", sender->balance, allocator);

	// json array object
	Value all_transfers_array;
	all_transfers_array.SetArray();

	string sender_id = sender->user_id;
	for(size_t i = 0; i < m_db->transfers.size(); i++){
		Transfer* information = m_db->transfers[i];
		if((sender_id == information->from->user_id) || (sender_id == information->to->user_id)){
			Value element;
			element.SetObject();
			element.AddMember("from", information->from->username, allocator);
			element.AddMember("to", information->to->username, allocator);
			element.AddMember("amount", information->amount, allocator);
			all_transfers_array.PushBack(element, allocator);
		}
	}

	// add array to return object
	info.AddMember("transfers", all_transfers_array, allocator);

	document.Swap(info);
	StringBuffer buffer;
	PrettyWriter<StringBuffer> writer(buffer);
	document.Accept(writer);

	response->setContentType("application/json");
	response->setBody(buffer.GetString() + string("\n"));
}

/*
void response_creation(int balance, vector<Transfer*> transfers, HTTPResponse* response){
	Document document;
	Document::AllocatorType &allocator = document.GetAllocator();

	// user balance element for json
	Value info; // return object
	info.SetObject();
	info.AddMember("balance", balance, allocator);

	// json array object
	Value all_transfers_array;
	all_transfers_array.SetArray();

	for(Transfer* i : transfers){
		Value element;
		element.SetObject();
		element.AddMember("from", i->from->username, allocator);
		element.AddMember("to", i->to->username, allocator);
		element.AddMember("amount", i->amount, allocator);
		all_transfers_array.PushBack(element, allocator);
	}

	// add array to return object
	info.AddMember("transfers", all_transfers_array, allocator);

	document.Swap(info);
	StringBuffer buffer;
	PrettyWriter<StringBuffer> writer(buffer);
	document.Accept(writer);

	response->setContentType("application/json");
	response->setBody(buffer.GetString()  + string("\n"));
}
*/
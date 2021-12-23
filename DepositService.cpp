#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "DepositService.h"
#include "Database.h"
#include "ClientError.h"
#include "HTTPClientResponse.h"
#include "HttpClient.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;


DepositService::DepositService() : HttpService("/deposits") { }

void DepositService::post(HTTPRequest *request, HTTPResponse *response) {
	// use credit card to deposit money into account
	WwwFormEncodedDict request_body = request->formEncodedBody();
	string str_amount = request_body.get("amount");
	int amount;
	string stripe_token = request_body.get("stripe_token");
	
	// authentication
	User* user = getAuthenticatedUser(request);
	if(user == NULL){
		throw ClientError::unauthorized();
	}

	if(str_amount.empty()){
		throw ClientError::badRequest();
	}

	amount = stoi(str_amount);
	if(amount < 50 || amount < 0){
		throw ClientError::badRequest();
	}

	if(stripe_token.empty()){
		throw ClientError::badRequest();
	}

	if(stripe_token != "tok_visa"){
		throw ClientError::unauthorized();
	}


	// call from server to the Stripe API to deposit money from a card
	// the client gets the token from the Stripe Token API
	// in the server the token that the client  got is sent to the Stripe Charge API
	// the deposit is complete
	WwwFormEncodedDict body;
	body.set("amount", amount);
	body.set("currency", "usd"); 
	body.set("source", stripe_token);
	string encoded_body = body.encode();

	// call to stripe api
	HttpClient client("api.stripe.com", 443, true);
	client.set_basic_auth(m_db->stripe_secret_key, "");
	
	HTTPClientResponse *client_response = client.post("/v1/charges", encoded_body);
	
	// This method converts the HTTP body into a rapidjson document
	Document *d = client_response->jsonBody();
	string stripe_charge_id = (*d)["id"].GetString();
	// string status = (*d)["status"].GetString();

	if(!client_response->success()){
		throw ClientError::unauthorized();
	}else{
		user->balance += amount;
		int balance = user->balance;

		Deposit* deposit;
		deposit->to = user;
		deposit->amount = amount;
		deposit->stripe_charge_id = stripe_charge_id;
		m_db->deposits.push_back(deposit);

		Document document;
		Document::AllocatorType &allocator = document.GetAllocator();

		// user balance element for json
		Value info; // return object
		info.SetObject();
		info.AddMember("balance", balance, allocator);

		// json array object
		Value all_deposits_array;
		all_deposits_array.SetArray();

		for(size_t i = 0; i < m_db->deposits.size(); i++){
			Deposit*  db_deposit = m_db->deposits[i];
			if(deposit->to == db_deposit->to){
				Value element;
				element.SetObject();
				element.AddMember("to", db_deposit->to->username, allocator);
				element.AddMember("amount", db_deposit->amount, allocator);
				element.AddMember("stripe_charge_id", db_deposit->stripe_charge_id, allocator);
				all_deposits_array.PushBack(element, allocator);
			}
		}

		// add array to return object
		info.AddMember("deposits", all_deposits_array, allocator);

		document.Swap(info);
		StringBuffer buffer;
		PrettyWriter<StringBuffer> writer(buffer);
		document.Accept(writer);

		response->setContentType("application/json");
		response->setBody(buffer.GetString()  + string("\n"));
	}
}
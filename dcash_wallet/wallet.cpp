#define RAPIDJSON_HAS_STDSTRING 1

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <iomanip>

#include "WwwFormEncodedDict.h"
#include "HttpClient.h"

#include "rapidjson/document.h"

using namespace std;
using namespace rapidjson;

int API_SERVER_PORT = 8080;
string API_SERVER_HOST = "localhost";
string PUBLISHABLE_KEY = "";

string auth_token;
string user_id;

void ERROR();
void api_redirection(vector<string>);
void output(string);

void logout();

// auth services
void auth(vector<string>);
void auth_service_delete();
void auth_service_post(vector<string>);

// account services
void account_service_put(string);
void account_service_get();

// deposit services
void deposit_post(int, string);
string deposit_auth_token_get(string);
string get_token_encoded_body(vector<string>);

// transfer services
void transfer_post(string);
string transfer_encoded_body(int, string);


int dollar_to_cents(string);
double cents_to_dollars(string);


int main(int argc, char *argv[]) {
	stringstream config;
	int fd = open("config.json", O_RDONLY);
	if (fd < 0) {
		cout << "could not open config.json" << endl;
		exit(1);
	}
	
	int ret;
	char buffer[4096];
	while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
		config << string(buffer, ret);
	}
	
	Document d;
	d.Parse(config.str());
	API_SERVER_PORT = d["api_server_port"].GetInt();
	API_SERVER_HOST = d["api_server_host"].GetString();
	PUBLISHABLE_KEY = d["stripe_publishable_key"].GetString();

	if (argc > 2){
		ERROR();
	}

	// batch mode
	if(argc > 1){
		for(int clinput = 1; clinput < argc; clinput++){
			int batch_file = open(argv[clinput], O_RDONLY);
			if(batch_file < 0){
				ERROR();
			}

			vector<vector<string>> batch_commands;

			int read_index = -1;
			char read_buffer[1];

			vector<string> command;
			string word = "";

			while((read_index = read(batch_file, read_buffer, sizeof(read_buffer))) > 0){
				if(read_buffer[0] == '\n'){
					command.push_back(word);
					batch_commands.push_back(command);
					command.clear();
					word.clear();
				}
				else if(read_buffer[0] != ' '){
					word += read_buffer[0];
				}
				else if(read_buffer[0] == ' '){
					command.push_back(word);
					word.clear();
				}
			}

			// iterate through each line of the batch file which is a vector of vectors
			for (size_t index =  0; index < batch_commands.size(); index++){
				api_redirection(batch_commands[index]);
			}			
		}
	}
	else{
        // interactive mode
        vector<string> commands;
        commands.push_back("");

        while(commands[0] != "logout"){
            string prompt = "D$> ";
            write(STDOUT_FILENO, prompt.c_str(), prompt.size());

            string cl_input = "";
            int read_line = 0;
            int buffer_size = 1;
            char read_buffer[buffer_size];
            
            // read entire line of input
            while((read_line = read(STDIN_FILENO, read_buffer, buffer_size)) > 0){
                if(read_buffer[0] != '\n'){
                    cl_input += read_buffer[0];
                }
            }

            // split the line by spaces
            vector<string> line;
            string word = "";
            for(size_t i = 0; i < cl_input.size(); i++){
                char letter = cl_input[i];
                if(letter == ' '){
                    line.push_back(word);
                    word.clear();
                }
                else{
                    word += letter;
                }
            }

            if(word.size() != 0){
                line.push_back(word);
                word.clear();
            }

            api_redirection(line);  
        }
        return 0;
    }

}


void ERROR(){
	const string ERROR = "Error\n";
	write(STDOUT_FILENO, ERROR.c_str(), ERROR.size());
	exit(1);
}


void api_redirection(vector<string> commands){
	if(commands[0] == "auth"){
		auth(commands);
	}
	else if(commands[0] == "balance"){
		if(commands.size() != 1){
			ERROR();
		}
		else{
			account_service_get();
		}
	}
	else if(commands[0] == "deposit"){
		if(commands.size() != 6){
			ERROR();
		}
		else{
			int amount = dollar_to_cents(commands[1]);
			string encoded_body = get_token_encoded_body(commands);
			string deposit_token = deposit_auth_token_get(encoded_body);
			deposit_post(amount, deposit_token);
		}
	}
	else if(commands[0] == "send"){
		if(commands.size() != 2){
			ERROR();
		}
		else{
			string to = commands[1];
			int amount = dollar_to_cents(commands[2]);
			string encoded_body = transfer_encoded_body(amount, to);
			transfer_post(encoded_body);
		}
	}
	else if(commands[0] == "logout"){
		if(commands.size() != 1){
			ERROR();
		}
		else{
			logout();
		}
	}
	else{
		ERROR();
	}
}


double cents_to_dollars(string cents){
	double dollar = 0;
	int cents_integer = stoi(cents);
	dollar = cents_integer/100;
	return dollar;
}


int dollar_to_cents(string dollar){
	string dollar_string = "";
	string cent_string = "";
	
	size_t decimal_position = dollar.find(".");
	if(decimal_position == string::npos){
		// no decimal meaning all dollars
		return stoi(dollar) * 100;
	}

	for(size_t i = 0; i < decimal_position; i++){
		dollar_string.push_back(dollar[i]);
	}

	for(size_t i = decimal_position + 1; i < dollar.size(); i++){
		cent_string.push_back(dollar[i]);
	}

	int dollar_to_cent = stoi(dollar_string) * 100;
	int cent = stoi(cent_string);
	int answer = dollar_to_cent + cent;
	
	return answer;
}


void logout(){
	auth_service_delete();
	auth_token.clear();
	user_id.clear();
}


void auth_service_delete(){
	string auth_service_delete_path = "/auth-tokens/"  + auth_token;
	HttpClient client("localhost", API_SERVER_PORT, false);
				
	HTTPClientResponse* client_response = client.del(auth_service_delete_path);
	if(!client_response->success()){
		ERROR();
	}
}


void auth_service_post(vector<string> commands){
	WwwFormEncodedDict body;
	body.set("username", commands[1]);
	body.set("password", commands[2]);
	string email = commands[3];
	body.set("email", email);
	string encoded_body = body.encode();

	HttpClient client("localhost", API_SERVER_PORT, false);


	string path  = "/auth-tokens";
	HTTPClientResponse* client_response = client.post(path, encoded_body);
	if(!client_response->success()){
		ERROR();
	}
	
	Document *d = client_response->jsonBody();
	auth_token = (*d)["auth_token"].GetString();
	user_id = (*d)["user_id"].GetString();
	
	account_service_put(email);
}


void auth(vector<string> commands){
	if(commands.size() > 4 || commands.size() == 1){
		ERROR();
	}
	
	if(!user_id.empty()){
		// a user is already signed in
		logout();
	}

	//Log in or create acc / Display balance
	if(auth_token.empty()){
		// auth post
		auth_service_post(commands);
	}	
}


void account_service_put(string email){
	// update account information
	WwwFormEncodedDict put_body;
	put_body.set("email", email);
	string encoded_body = put_body.encode();
	
	string account_service_path = "/users/" + user_id;
	HttpClient acc_service("localhost", API_SERVER_PORT, false);
	acc_service.set_header("x-auth-token", auth_token);

	// response after updating account service
	HTTPClientResponse*  acc_service_response = acc_service.put(account_service_path, encoded_body);
	
	if(!acc_service_response->success()){
		ERROR();
	}

	// print balance to console
	Document* d = acc_service_response->jsonBody();
	string balance = (*d)["balance"].GetString();

	output(balance);
}


void account_service_get(){
	if(auth_token.empty() || user_id.empty()){
		ERROR();
	}

	string account_service_path = "/users/" + user_id;
	HttpClient acc_service("localhost", API_SERVER_PORT, false);
	acc_service.set_header("x-auth-token", auth_token);

	// response after updating account service
	HTTPClientResponse*  acc_service_response = acc_service.put(account_service_path, "");
	if(!acc_service_response->success()){
		ERROR();
	}

	// print balance to console
	Document* d = acc_service_response->jsonBody();
	string balance = (*d)["balance"].GetString();
	output(balance);
}


string deposit_auth_token_get(string encoded_body){
	HttpClient client("api.stripe.com", 443, true);
	client.set_header("Authorization", string("Bearer  ") + PUBLISHABLE_KEY);
	HTTPClientResponse* client_response = client.post("/v1/tokens", encoded_body);
	
	if(!client_response->success()){
		ERROR();
	}

	Document *d = client_response->jsonBody();
	string token = (*d)["id"].GetString();

	return token;
}


string get_token_encoded_body(vector<string> command){
	WwwFormEncodedDict body;
	body.set("card[number]", command[2]);
	body.set("card[exp_year]", command[3]);
	body.set("card[exp_month]", command[4]);
	body.set("card[cvc]", command[5]);
	return body.encode();
}


void deposit_post(int amount, string stripe_token){
	WwwFormEncodedDict body;
	body.set("amount", amount);
	body.set("stripe_token", stripe_token);
	string encoded_body = body.encode();

	HttpClient client("localhost", API_SERVER_PORT, false);
	client.set_header("x-auth-token", auth_token);
	
	string path = "/deposits";
	HTTPClientResponse* client_response = client.post(path, encoded_body);
	if(!client_response->success()){
		ERROR();
	}

	Document *d = client_response->jsonBody();
	string balance = (*d)["balance"].GetString();

	output(balance);
}


void output(string balance){
	string parsed_balance = to_string(cents_to_dollars(balance)); 
	cout << "Balance: $" << fixed << setprecision(2) << parsed_balance << endl;
}


string transfer_encoded_body(int amount, string to){
	WwwFormEncodedDict body;
	body.set("to", to);
	body.set("amount", amount);
	return body.encode();
}


void transfer_post(string encoded_body){
	HttpClient client("localhost", API_SERVER_PORT, false);
	client.set_header("x-auth-token", auth_token);
	
	string path = "/transfers";
	HTTPClientResponse* client_response = client.post(path, encoded_body);
	if(!client_response->success()){
		ERROR();
	}

	Document* d = client_response->jsonBody();
	string balance = (*d)["balance"].GetString();

	output(balance);
}

// A C++ file to read in a TradingSession dat file and utilize it as a Product struct
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <algorithm>

using namespace std;

// A subset of TradeDate, containing info from each Session (there are several per day)
struct Session {
    string utc_timestamp = ""; //341
    int trading_session_id = 0;  //336
    // Enumeration of 336 - 21: Pre-Open; 17: Ready to trade; 2: Trading halt(Pause);
    // 18: Not available for trading; 4: Close; 26: Post Close;
    int trading_session_interrupt = 0;  // [625] (only available when trading_session_id = 21)
};

// A subset of Product, containing information from each day from the week schedule
struct TradeDate {
    vector<Session> sessions;
    string trade_date_string = ""; // YYYYMMDD 75
    int no_trading_sessions = 0; // 386
};

// The fundamental container of the Project - Holds information from the file
struct Product {
    vector<TradeDate> trade_dates;
    string product_complex = "";  //1227
    string security_group = "";  // 1151
    int market_segment_id = 0;  //1300
    int no_dates = 0;  // 580
};

// Just a way to debug - lists some of the information on Products. Can be modified to show other stuff without issue.
// Did you know you can't append endl to a string? And also two string literals. This should have been like a two minute
// function, but it took ages cause of that. C++ is weird.
string ProductToString(Product my_prod){
    string full_string = my_prod.product_complex
            + string("\n\tSecurityGroup: ") + my_prod.security_group
            + string("\n\tNoDates: ") + to_string(my_prod.no_dates)
            + string("\n\tFirst Date: ") + my_prod.trade_dates.at(0).trade_date_string;
    return full_string;
}

string DateToString(TradeDate my_date){
    vector<Session>::iterator it;
    string full_string = my_date.trade_date_string + "\n\t";
    for(it = my_date.sessions.begin(); it != my_date.sessions.end(); it++){
        Session sesh = *it;
        full_string += sesh.utc_timestamp + " " + to_string(sesh.trading_session_id) + "\n\t";
    }
    return full_string;
}

// Grabs the next substring up to the next SOH line, after the = sign
string GetNextChunk(string line){
    if(line.empty()){
        return nullptr;
    }
    else{
        int pos;
        pos = line.find(0x1);
        //Once again, the line should have one if it's non-empty, but lord only knows
        if(pos == string::npos){
            cout << "Wasn't able to find the terminating character. Your file is in the wrong format." << endl;
            return nullptr;
        }
        string chunk = line.substr(0, pos);
        pos = line.find("=");
        chunk = chunk.substr(pos + 1, chunk.length());

        return chunk;
    }
}

// Grabs the next label (substring up to SOH, but behind the = sign)
string GetNextLabel(string line){
    if(line == ""){
        return "done";
    }
    else{
        int pos;
        pos = line.find(0x1);
        //Once again, the line should have one if it's non-empty, but lord only knows
        if(pos == string::npos){
            cout << "Wasn't able to find the terminating character. Your file is in the wrong format." << endl;
            return nullptr;
        }
        string chunk = line.substr(0, pos);
        pos = line.find("=");
        chunk = line.substr(0, pos);
        return chunk;
    }
}

// Chomps off from [0] to the index after the next SOH. So like, Del("1=abc[]2=efgh[]3=sadkf[]") = "2=efgh[]3=sadkf[]"
string DelUpToNextItem(string line){
    if(line == ""){
        return nullptr;
    }
    else {
        int pos;
        pos = line.find(0x1);
        //Once again, the line should have one if it's non-empty, but lord only knows
        if(pos == string::npos){
            cout << "Wasn't able to find the terminating character. Your file is in the wrong format." << endl;
            return nullptr;
        }
        string rest = line.substr(pos + 1, line.length());
        return rest;
    }
}

//Checks to see if a vector contains a string
bool ContainsString(vector<string> vec, string to_find){
    vector<string>::iterator it;
    for(it = vec.begin(); it != vec.end(); it++){
        if(*it == to_find){
            return true;
        }
    }
    return false;
}

// Checks to see if the given date actually includes a half day or holiday
bool IsThreeDayDate(TradeDate date){
    vector<string> dates;
    vector<Session>::iterator it;

    for(it = date.sessions.begin(); it != date.sessions.end(); it++){
        Session a_session = *it;
        string today_string = a_session.utc_timestamp.substr(0, 8);

        if (! ContainsString(dates, today_string)){
            dates.push_back(today_string);
        }
    }

    // Now we just need to know how many days there are
    if(dates.size() == 3){
        return true;
    }
    return false;
}

// Returns a string - enumeration ["Same Schedule", "Closed on the holiday",
// "Different Number of Sessions", "A Session Differed"]
// Diffs the two products to figure out which to show
string IsSimilar(Product normal_day, Product holiday){
    int ii = 0;

    while(ii < normal_day.no_dates){
        TradeDate a_date = normal_day.trade_dates.at(ii);
        TradeDate b_date = holiday.trade_dates.at(ii);
        ii++;

        // If there's an extra day built in, it was a half day
        if(IsThreeDayDate(b_date)){
            return "Had a half day";
        }
        else if(holiday.no_dates < normal_day.no_dates){
            return "The whole day was off";
        }

        // To be safe, we should make sure that noSessions is different
        if(a_date.no_trading_sessions != b_date.no_trading_sessions){
            return "Different Number of Sessions";
        }
        else{
            int jj = 0;
            while(jj < a_date.no_trading_sessions){
                // Then all that's rest to do is, I guess, diff the sessions
                Session a_sesh = a_date.sessions.at(jj);
                Session b_sesh = b_date.sessions.at(jj);
                jj++;

                // Order should be maintained across similar days
                if(a_sesh.trading_session_id != b_sesh.trading_session_id){
                    return "A Session Differed";
                }
            }
        }
    }

    return "Same Schedule";
}


// creates a map from a product security group (Product.security_group) to the Product itself. That way I can loop
// through the list of products fairly easily
map<string, Product> MakeProductMap(string file_name) {
    //Read in the file provided by the first argument
    ifstream my_file(file_name);
    map<string, Product> prod_list;

    if (my_file) {
        // For each line in the file, add it to a map
        // For now I'll sort by the prod description, but that can be tweaked
        string my_line;

        while(getline(my_file, my_line)){
            // Delete the first chunk 35
            Product my_product;
            my_line = DelUpToNextItem(my_line);
            my_product.market_segment_id = stoi(GetNextChunk(my_line));
            my_line = DelUpToNextItem(my_line);
            my_product.product_complex = GetNextChunk(my_line);
            my_line = DelUpToNextItem(my_line);
            my_product.security_group = GetNextChunk(my_line);
            my_line = DelUpToNextItem(my_line);
            my_product.no_dates = stoi(GetNextChunk(my_line));
            my_line = DelUpToNextItem(my_line);
            vector<TradeDate> dates;
            int i = 0;

            while(i < my_product.no_dates){
                TradeDate this_date;
                this_date.trade_date_string = GetNextChunk(my_line);
                my_line = DelUpToNextItem(my_line);
                this_date.no_trading_sessions = stoi(GetNextChunk(my_line));
                my_line = DelUpToNextItem(my_line);
                vector<Session> sessions;
                int j = 0;

                while(j < this_date.no_trading_sessions){
                    Session this_session;
                    this_session.trading_session_id = stoi(GetNextChunk(my_line));
                    my_line = DelUpToNextItem(my_line);
                    this_session.utc_timestamp = GetNextChunk(my_line);
                    my_line = DelUpToNextItem(my_line);
                    string nextLabel = GetNextLabel(my_line);
                    if(nextLabel == "625"){
                        this_session.trading_session_interrupt = stoi(GetNextChunk(my_line));
                        my_line = DelUpToNextItem(my_line);
                    }
                    else{
                        this_session.trading_session_interrupt = 0;
                    }

                    sessions.push_back(this_session);
                    j ++;
                }
                this_date.sessions = sessions;
                dates.push_back(this_date);
                i++;
            }

            my_product.trade_dates = dates;
            prod_list[my_product.security_group] = my_product;
        }
    }
    else{
        cout << "Failed to open file." << endl;
    }


    return prod_list;
}

int main(){
    map<string, Product> normal =
            MakeProductMap("C:\\Users\\Grift\\Desktop\\readInTradingSessionList\\dataFiles\\TradingSessionList.dat");
    map<string, Product> holiday =
            MakeProductMap("C:\\Users\\Grift\\Desktop\\readInTradingSessionList\\dataFiles\\TradingSessionListMemorialDay.dat");

    map<string, Product>::iterator it;

    // Testing the IsThreeDayDate function
    /*
    Session a, b, c, d;
    a.utc_timestamp = "2018021522000000";
    a.trading_session_id = 99;
    b.utc_timestamp = "2018021608000000";
    b.trading_session_id = 99;
    c.utc_timestamp = "2018021622000000";
    c.trading_session_id = 99;
    d.utc_timestamp = "2018021722000000";
    d.trading_session_id = 99;

    vector<Session> a_sesh;
    vector<Session> b_sesh;
    vector<Session> c_sesh;
    // Should return True
    a_sesh.push_back(a);
    a_sesh.push_back(b);
    a_sesh.push_back(c);
    a_sesh.push_back(d);
    TradeDate a_date;
    a_date.sessions = a_sesh;
    // Should return false
    b_sesh.push_back(a);
    b_sesh.push_back(b);
    b_sesh.push_back(c);
    TradeDate b_date;
    b_date.sessions = b_sesh;
    // Should return false, easier case
    c_sesh.push_back(a);
    c_sesh.push_back(b);
    TradeDate c_date;
    c_date.sessions = c_sesh;

    cout << IsThreeDayDate(a_date) << endl;
    cout << IsThreeDayDate(b_date) << endl;
    cout << IsThreeDayDate(c_date) << endl;
    */


    for (it = normal.begin(); it != normal.end(); it++) {
        string getter = it->first;
        Product normal_prod = normal[getter];
        Product holiday_prod = holiday[getter];
        string well_is_it = IsSimilar(normal_prod, holiday_prod);

        cout << normal_prod.product_complex << endl;
        cout << "   " << well_is_it << endl;
    }


    return 0;
}
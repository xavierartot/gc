var Firebase = require('firebase');
var util = require('util');
var q = require('promised-io/promise');

var GcInfluxData = function(influx_client, firebase_root, username, uid, device_id) {
    
    var firebase_root_ref = new Firebase(firebase_root);
    this.firebase_data_latest_ref = firebase_root_ref.child('data').child(uid).child('latest');
    this.device_ref = firebase_root_ref.child('devices').child(device_id);

    this.client = influx_client;
    this.username = username;
    this.uid = uid;
    
    var self = this;
    
    
    
    this.update_latest_data = function() {
        
        self.get_current_night_intervals().then(function(intervals) {
            self.compute_total_score(intervals.time_clause).then(function(total_score) {
               console.log("total_score: ", total_score);
               
               // update firebase
               self.firebase_data_latest_ref.update({
                  start_timestamp: intervals.start_timestamp,
                  end_timestamp: intervals.end_timestamp,
                  total_score: total_score,
                  collected_duration: intervals.collected_duration,
                  last_update_time: Firebase.ServerValue.TIMESTAMP
               });
            });            
        });
        

        
    };
    
    this.get_current_night_intervals = function() {
        var defer = q.defer();
        
        self.device_ref.once('value', function(snapshot) {
            var data = snapshot.val();
            var start_timestamp = data.collection_start;
            var end_timestamp = data.last_upload_time;
            var start_date = new Date(start_timestamp);
            var end_date = new Date(end_timestamp);
            
            var result = {
                "start_timestamp": start_timestamp,
                "end_timestamp": end_timestamp,
                "time_clause": "time > '" + start_date.toISOString() + "' and time < '" + end_date.toISOString() + "'",
                "collected_duration": data.collected_duration
            }
            
            defer.resolve(result);
        })
        
        return defer.promise;
    };
    
    this.compute_total_score = function(time_clause) {
        var defer = q.defer();
        

        console.log("compute_total_score, time_clause: ", time_clause);
        
	    var query_percentile = "select percentile(emg_value,85) from emg where " + time_clause + " and username='" + self.username + "'";	
	    console.log("query_percentile: ", query_percentile);
	
	    // perform query here
	    self.client
            .query(query_percentile)
            .then(function(result) {
                console.log(util.inspect(result, {showHidden: false, depth: null}));
                var threshold_value = result.results[0].series[0].values[0][1];
                console.log("threshold_value: ", threshold_value);
                
                var query_sum = "select sum(emg_value) from emg where " + time_clause + " and emg_value > " + 
                                threshold_value + " and username='" + self.username + "'";
                self.client.query(query_sum).then(function(result) {
                    console.log(util.inspect(result, {showHidden: false, depth:null}));                                        
                    var sum = result.results[0].series[0].values[0][1];
                    console.log("sum: ", sum);
                    
                    var query_count = "select count(emg_value) from emg where " + time_clause + " and emg_value > " + 
                        threshold_value + " and username='" + self.username + "'";
                    self.client.query(query_count).then(function(result) {
                        console.log(util.inspect(result, {showHidden: false, depth:null}));
                        var count = result.results[0].series[0].values[0][1];
                        console.log("count: ", count);
                        
                        var total_score = sum - count * threshold_value;
                        console.log("total_score: ", total_score);
                        
                        defer.resolve(total_score);
                        
                    });
                });
                
            }, function(error) {
            	console.log("error: ", error);
            	
            });
    
        return defer.promise;
    };
    
    return this;
};

module.exports = GcInfluxData;

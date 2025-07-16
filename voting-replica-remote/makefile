// Usage: ./server num_voters redo vote_mode(0: ICED 0: IDEC: 1 EDEC: 2) total_rounds powersave_mode fm fa(bvn) sprd
launch-server:
	./server 5 1 2 5 0 1 1 5 

// Usage: ./client num_voters voter_id powersave_mode_enabled(0 to disable) fault_severity
// for each "client" machine where you execute the client script,
// remember to increment the "voter_id" parameter.
launch-client-0:
	./client 5 0 0 0.6

compile-server:
	g++ -o server vote_server_anyN_fa_gt_fm.cpp

compile-client:
	g++ -o client vote_client_anyN_fa_gt_fm.cpp
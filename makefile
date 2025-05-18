all: treasure_hub treasure_monitor treasure_manager calculate_score

treasure_hub:
	gcc -Wall -o treasure_hub treasure_hub.c

treasure_monitor:
	gcc -Wall -o monitor treasure_monitorv2.c

treasure_manager:
	gcc -Wall -o treasure_manager treasure_manager.c

calculate_score:
	gcc -Wall -o calculate_score calculate_score.c

clean:
	rm -f treasure_hub monitor treasure_manager calculate_score

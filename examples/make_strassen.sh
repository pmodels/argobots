rm -f strassen_thread
#rm -f strassen_task
rm -f simple_omptask
rm -f abt_with_abt_test
clang -g -O2 nested_abt.c -lm -I/home/poornimans/argobots-install/include -L/home/poornimans/argobots-install/lib -labt -o nested_abt
clang -g abt_with_abt_test.c -I/home/poornimans/argobots-install/include -L/home/poornimans/argobots-install/lib -labt -o abt_with_abt_test
clang -g simple_omptask.c -I/home/poornimans/argobots-install/include -I/home/poornimans/bolt-install/include -L/home/poornimans/argobots-install/lib -L/home/poornimans/bolt-install/lib -fopenmp -labt -o simple_omptask
#clang -g strassen_omp_task.c -lm -I/home/poornimans/argobots-install/include -I/home/poornimans/bolt-install/include -L/home/poornimans/argobots-install/lib -L/home/poornimans/bolt-install/lib -fopenmp -labt -o strassen_task
clang -g strassen_thread.c -lm -I/home/poornimans/argobots-install/include -L/home/poornimans/argobots-install/lib -labt -o strassen_thread

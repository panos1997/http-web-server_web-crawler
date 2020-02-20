# web-server-web-crawler, written in C

## This project is made for academic purposes and it consists of 3 parts:

# Part 1 - Web site creator
#### The bash script webcreator.sh creates random text websites. It is run with the command:
### ./webcreator.sh root_directory text_file w p
* root_directory -> an empty directory in which the websites will be created
* text_file      -> a text file from which random words will be chosen to fill the html pages of the websites
* w              -> number of websites
* p              -> number of webpages in each website

# Part 2 - Web server
#### The server runs with this command: 
### ./myhttpd -p serving_port -c command_port -t num_of_threads -d root_dir

* serving_port      -> here the server listens in order to return the html pages
* command_port      -> in this port we give some instructions to the server 
  * STATS    -> return the time the server is up and running, how many pages are returned and the total number of bytes returned.
  * SHUTDOWN -> server frees memory and stops running 

# Part 3 - Web crawler
#### The crawler follows the links that are inside each html page and downloads all the pages. It runs with:

### ./mycrawler -h host_or_IP -p port -c command_port -t num_of_threads -d save_dir starting_URL

* host_or_ip     -> where the server runs
* command_port   -> port the server listens to
* command_port   -> port where we enter commands for the crawler
* num_of_threads -> number of crawler threads
* save_dir       -> where it saves the downloaded pages
* starting_URL   -> the url the crawler starts crawling             

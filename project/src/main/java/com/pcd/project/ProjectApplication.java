package com.pcd.project;


import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.http.*;
import org.springframework.http.client.SimpleClientHttpRequestFactory;
import org.springframework.http.converter.HttpMessageConverter;
import org.springframework.http.converter.json.MappingJackson2HttpMessageConverter;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;
import org.springframework.web.client.RestTemplate;
import org.springframework.web.reactive.function.BodyInserter;
import org.springframework.web.reactive.function.BodyInserters;
import org.springframework.web.reactive.function.client.WebClient;
import reactor.core.publisher.Mono;

import java.io.*;
import java.util.*;

@SpringBootApplication
public class ProjectApplication {
	private static final String path = "http://localhost:7588";
	static SimpleClientHttpRequestFactory clientHttpRequestFactory = new SimpleClientHttpRequestFactory();
	static RestTemplate restTemplate;
	//static RestTemplate restTemplate = new RestTemplate(clientHttpRequestFactory);

	public static String sendRequestType(HttpHeaders headers, String data){
		HttpEntity<String> entity = new HttpEntity<>(data, headers);
		//restTemplate.postForObject(path, entity, String.class);
		//String response = restTemplate.postForObject(path, entity, String.class);
		String response = restTemplate.exchange(path, HttpMethod.POST, entity, String.class).getBody();
		System.out.flush();
		for(int i = 72; i < response.length(); i++)
			System.out.print(response.charAt(i));

//		System.out.println((restTemplate.exchange(path, HttpMethod.POST, entity, String.class).getBody())+ "\n");
//		System.out.flush();
		return response;
	}

	public static void main(String[] args) throws FileNotFoundException{
		//SpringApplication.run(ProjectApplication.class, args);

		clientHttpRequestFactory.setConnectTimeout(3000);
		//restTemplate = new RestTemplate(clientHttpRequestFactory);
		restTemplate = new RestTemplate();
//		List<HttpMessageConverter<?>> messageConverters = new ArrayList<HttpMessageConverter<?>>();
//		MappingJackson2HttpMessageConverter converter = new MappingJackson2HttpMessageConverter();
//		converter.setSupportedMediaTypes(Collections.singletonList(MediaType.TEXT_PLAIN));
//		messageConverters.add(converter);
//
//		restTemplate.setMessageConverters(messageConverters);
		HttpHeaders headers = new HttpHeaders();
		headers.setAccept(Arrays.asList(MediaType.ALL));

		Scanner input = new Scanner(System.in);
		int opt, cont = 0, id, i, j, k, l, songSize, requestId, octSize, packetsNo, actualSongSize, toSendSize, dataSize = 1024;
		File audio;
		String data, songTitle, songGenre, sentNameGenre, auxSize = "", auxRequestId = "", auxOct = "", songbytes, songPath, sentAudio;

		while (cont == 0) {
			System.out.println("\nChoose an option:\n1 - Listen to a song\n2 - Check uploads\n3 - Upload file\n");
			opt = input.nextInt();
			switch (opt) {
				case 1:
					actualSongSize = 0;
					System.out.println("\nThe ID of the wanted song: ");
					id = input.nextInt();
					songTitle = String.format("song%d.mp3", id);
					data = String.format("---1---%d", id);
					data = sendRequestType(headers, data);

					j = 0;
					for(i = 4; i < 8; i++){
						if(Character.isDigit(data.charAt(i))) {
							auxSize.concat(String.valueOf(data.charAt(i)));
							j++;
						}
					}
					songSize = Integer.valueOf(auxSize);

					j = 0;
					for(i = 8; i < 12; i++){
						if(Character.isDigit(data.charAt(i))) {
							auxRequestId.concat(String.valueOf(data.charAt(i)));
							j++;
						}
					}
					requestId = Integer.valueOf(auxRequestId);

					data = String.format("---%d---%d---%d", 6, requestId, requestId);
					data = sendRequestType(headers, data);

					PrintWriter writer = new PrintWriter(songTitle);
					for(i = 0; i < songSize; i++){
						writer.print(data);
						data = String.format("---%d---%d---%d", 6, requestId, requestId);
						data = sendRequestType(headers, data);
					}
					writer.close();

					break;

				case 2:
					data = String.format("---%d", 2);
					sendRequestType(headers, data);
					//System.out.println(data);

					//requestId = Integer.valueOf(data.charAt(11));
					//data = String.format("---%d---%d---%d", 6, 0, 0);
					//data = sendRequestType(headers, data);
					//WebClient client = WebClient.create(path);
					//WebClient client = WebClient.builder().baseUrl(path).build();
//					Mono<String> response = webClient.post()
//							.uri(path)
//							.header(HttpHeaders.CONTENT_TYPE, MediaType.APPLICATION_JSON_VALUE)
//							.body(Mono.just(data), String.class)
//							.retrieve()
//							.bodyToMono(String.class);
					//Mono<String> response =
//					client.post().uri(path).contentType(MediaType.TEXT_PLAIN).body(BodyInserters.fromValue(data))
//							.retrieve().bodyToMono(String.class).subscribe(System.out::println);
					//System.out.println(response.toString());
//					client.post()
//							.uri(path)
//							.body(Mono.just(data), String.class)
//							.retrieve()
//							.bodyToFlux(String.class)
//							.subscribe(System.out::println);

					break;
			}
			System.out.println("\nDo you want to continue?\n0 - Yes\n1 - No\n");
			cont = input.nextInt();
		}
	}
}
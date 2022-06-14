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
		System.out.println(response);

//		System.out.println((restTemplate.exchange(path, HttpMethod.POST, entity, String.class).getBody())+ "\n");
//		System.out.flush();
		return response;
	}

	public static void main(String[] args) throws FileNotFoundException, UnsupportedEncodingException {
		//SpringApplication.run(ProjectApplication.class, args);

		//clientHttpRequestFactory.setReadTimeout(10000);
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
		String data, songTitle, songGenre, sentNameGenre, auxSize = "", auxRequestId = "", auxOct = "", songbytes, path, sentAudio;

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

					l = 0;
					PrintWriter writer = new PrintWriter(songTitle);
					for(i = 0; i < songSize; i++){
						j = 0;
						for(k = 4; k < 8; k++){
							auxOct.concat(String.valueOf(data.charAt(k)));
							j++;
						}
						octSize = Integer.valueOf(auxOct);
						writer.print(data);
						data = String.format("---%d---%d---%d", 6, requestId, requestId);
						data = sendRequestType(headers, data);
					}

					break;

				case 2:
					data = String.format("---%d", 2);
					data = sendRequestType(headers, data);
					//System.out.println(data);

					//requestId = Integer.valueOf(data.charAt(11));
					data = String.format("---%d---%d---%d", 6, 0, 0);
					//data = sendRequestType(headers, data);
					break;
			}
			System.out.println("\nDo you want to continue?\n0 - Yes\n1 - No\n");
			cont = input.nextInt();
		}
	}
}
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
		//String response = restTemplate.exchange(path, HttpMethod.POST, entity, String.class).getBody();
		//System.out.println(response);

		System.out.println(restTemplate.exchange(path, HttpMethod.POST, entity, char.class).getBody());
		return null;
	}

	public static void main(String[] args) throws IOException {
		//clientHttpRequestFactory.setReadTimeout(3000);
		restTemplate = new RestTemplate(clientHttpRequestFactory);
//		List<HttpMessageConverter<?>> messageConverters = new ArrayList<HttpMessageConverter<?>>();
//		MappingJackson2HttpMessageConverter converter = new MappingJackson2HttpMessageConverter();
//		converter.setSupportedMediaTypes(Collections.singletonList(MediaType.ALL));
//		messageConverters.add(converter);
//		restTemplate.setMessageConverters(messageConverters);
		SpringApplication.run(ProjectApplication.class, args);
		HttpHeaders headers = new HttpHeaders();
		//headers.setAccept(Arrays.asList(MediaType.TEXT_PLAIN));
		headers.setAccept(Arrays.asList(MediaType.ALL));

		Scanner input = new Scanner(System.in);
		int opt, cont = 0, id, i, j, k, l, songSize, requestId, octSize, packetsNo, actualSongSize, toSendSize, dataSize = 1024;
		File audio;
		String data, songTitle, songGenre, sentNameGenre, auxSize, auxRequestId, auxOct, songbytes, path, sentAudio;

		while (cont == 0) {
			System.out.println("\nChoose an option:\n1 - Listen to a song\n2 - Check uploads\n3 - Upload file\n");
			opt = input.nextInt();
			switch (opt) {
				case 1:
					actualSongSize = 0;
					System.out.println("\nThe ID of the wanted song: ");
					id = input.nextInt();

					break;

				case 2:
					data = String.format("---%d", 2);
					data = sendRequestType(headers, data);
					System.out.println(data);

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
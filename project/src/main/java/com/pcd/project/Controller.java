package com.pcd.project;

import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.reactive.function.client.WebClient;
import reactor.core.publisher.Mono;

@RestController
public class Controller {
    private static final String path = "http://localhost:7588";
    WebClient client = WebClient.create(path);
    WebClient webClient = WebClient.builder()
            .baseUrl(path)
            .defaultHeader(HttpHeaders.CONTENT_TYPE, MediaType.APPLICATION_JSON_VALUE)
            .build();
    String data = data = String.format("---%d", 2);
    Mono<String> response = webClient.post()
            .uri(path)
            .header(HttpHeaders.CONTENT_TYPE, MediaType.APPLICATION_JSON_VALUE)
            .body(Mono.just(data), String.class)
            .retrieve()
            .bodyToMono(String.class);
}